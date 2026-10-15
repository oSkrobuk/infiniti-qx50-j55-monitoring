#include "BleManager.h"

#include <algorithm>
#include <limits.h>
#include <string.h>
#include <vector>

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>

#include "AlertManager.h"
#include "Version.h"

namespace {

static constexpr char UUID_SERVICE[] = "f0a0b100-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_DEVICE_INFO[] = "f0a0b101-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_CONTROL[] = "f0a0b102-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_STATUS[] = "f0a0b103-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_HISTORY[] = "f0a0b104-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_LIVE[] = "f0a0b105-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_ALERTS[] = "f0a0b106-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_SUMMARY[] = "f0a0b107-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_METRIC_SCHEMA[] = "f0a0b108-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr char UUID_AUTH_CHALLENGE[] = "f0a0b109-9c4e-4b7a-9d1e-2f6c8a3b0001";
static constexpr uint16_t HISTORY_CAPACITY = 1800;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t HISTORY_RETRY_MS = 5000;
static constexpr uint32_t HISTORY_FLUSH_MS = 10000;
static constexpr uint8_t LIVE_PERIOD_25MS = 40;
static constexpr char AUTH_PREFIX[] = "KANVEX-AUTH-V5";
static constexpr uint16_t ADV_INTERVAL_UNITS = 160;
static constexpr uint32_t ADV_REFRESH_MS = 500;
static constexpr uint8_t ADV_FLAG_ENGINE_RUNNING = 1 << 0;
static constexpr uint8_t ADV_FLAG_UNACKED_RECORDS = 1 << 1;
static constexpr uint8_t ADV_FLAG_SUMMARIES_PENDING = 1 << 2;
static constexpr uint8_t ADV_FLAG_PAIRED = 1 << 3;
static constexpr uint8_t SUMMARY_FLAG_TIME_KNOWN = 1 << 0;

BleManager *s_manager = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *) override
    {
        if (s_manager != nullptr) s_manager->on_connect();
    }

    void onDisconnect(NimBLEServer *) override
    {
        if (s_manager != nullptr) s_manager->on_disconnect();
    }
};

class DeviceInfoCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *) override
    {
        if (s_manager != nullptr) s_manager->on_device_info_read();
    }
};

class ChallengeCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *) override
    {
        if (s_manager != nullptr) s_manager->on_challenge_read();
    }
};

class ControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic) override
    {
        if (s_manager == nullptr) return;
        const std::string value = characteristic->getValue();
        s_manager->on_control_write(reinterpret_cast<const uint8_t *>(value.data()), value.size());
    }
};

static uint32_t load_boot_counter()
{
    Preferences preferences;
    if (!preferences.begin("kanvex_ble", false)) return 1;
    const uint32_t value = preferences.getUInt("boot", 0) + 1;
    preferences.putUInt("boot", value);
    preferences.end();
    return value;
}

static bool load_token(uint8_t token[PROTOCOL_TOKEN_SIZE])
{
    Preferences preferences;
    if (!preferences.begin("kanvex_ble", true)) return false;
    const bool found = preferences.getBytesLength("token") == PROTOCOL_TOKEN_SIZE &&
                       preferences.getBytes("token", token, PROTOCOL_TOKEN_SIZE) == PROTOCOL_TOKEN_SIZE;
    preferences.end();
    return found;
}

static bool save_token(const uint8_t token[PROTOCOL_TOKEN_SIZE])
{
    Preferences preferences;
    if (!preferences.begin("kanvex_ble", false)) return false;
    const bool saved = preferences.putBytes("token", token, PROTOCOL_TOKEN_SIZE) == PROTOCOL_TOKEN_SIZE;
    preferences.end();
    return saved;
}

static bool save_time_anchor(uint64_t unix_ms, uint32_t uptime_ms, uint32_t boot_counter)
{
    struct TimeAnchor {
        uint64_t unix_ms;
        uint32_t uptime_ms;
        uint32_t boot_counter;
    } anchor = {unix_ms, uptime_ms, boot_counter};
    Preferences preferences;
    if (!preferences.begin("kanvex_ble", false)) return false;
    const bool saved = preferences.putBytes("time_anchor", &anchor, sizeof(anchor)) == sizeof(anchor);
    preferences.end();
    return saved;
}

static uint16_t firmware_version_wire()
{
    unsigned int major = 0;
    unsigned int minor = 0;
    if (sscanf(FW_VERSION, "%*u.%u.%u", &major, &minor) != 2) return 0;
    return static_cast<uint16_t>(((major & 0xFFu) << 8) | (minor & 0xFFu));
}

static int16_t metric_average(int64_t sum, uint32_t count)
{
    if (count == 0) return static_cast<int16_t>(PROTOCOL_NO_VALUE_SIGNED);
    const int64_t divisor = static_cast<int64_t>(count);
    const int64_t magnitude = sum >= 0 ? sum : -sum;
    const int64_t rounded = (2 * magnitude + divisor) / (2 * divisor);
    return static_cast<int16_t>(sum >= 0 ? rounded : -rounded);
}

} // namespace

BleManager ble_manager;

bool BleManager::begin()
{
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Serial.println("[BLE] Не удалось создать мьютекс");
        return false;
    }
    if (!history_.init(HISTORY_CAPACITY)) {
        Serial.println("[BLE] Не удалось выделить буфер истории");
        return false;
    }

    boot_counter_ = load_boot_counter();
    paired_ = load_token(token_);
    load_summaries_();
    reset_trip_stats_();
    observed_alert_index_ = alert_manager.event_next_index();
    const uint64_t chip_id = ESP.getEfuseMac();
    uint32_t hash = 2166136261u;
    for (uint8_t i = 0; i < sizeof(chip_id); ++i) {
        hash ^= static_cast<uint8_t>(chip_id >> (i * 8));
        hash *= 16777619u;
    }
    snprintf(device_name_, sizeof(device_name_), "QX50M_%04lx", static_cast<unsigned long>(hash & 0xFFFFu));

    s_manager = this;
    NimBLEDevice::init(device_name_);
    NimBLEDevice::setMTU(517);
    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(new ServerCallbacks());
    NimBLEService *service = server_->createService(UUID_SERVICE);

    device_info_characteristic_ = service->createCharacteristic(UUID_DEVICE_INFO, NIMBLE_PROPERTY::READ);
    device_info_characteristic_->setCallbacks(new DeviceInfoCallbacks());

    NimBLECharacteristic *schema = service->createCharacteristic(UUID_METRIC_SCHEMA, NIMBLE_PROPERTY::READ);
    uint8_t schema_data[PROTOCOL_SIZE_METRIC_SCHEMA_HEADER + sizeof(METRIC_DESCRIPTORS)] = {};
    const uint16_t schema_size = ble_encode_metric_schema(schema_data, sizeof(schema_data));
    schema->setValue(schema_data, schema_size);

    challenge_characteristic_ = service->createCharacteristic(UUID_AUTH_CHALLENGE, NIMBLE_PROPERTY::READ);
    challenge_characteristic_->setCallbacks(new ChallengeCallbacks());

    NimBLECharacteristic *control = service->createCharacteristic(UUID_CONTROL, NIMBLE_PROPERTY::WRITE);
    control->setCallbacks(new ControlCallbacks());

    status_characteristic_ = service->createCharacteristic(UUID_STATUS, NIMBLE_PROPERTY::NOTIFY);
    history_characteristic_ = service->createCharacteristic(UUID_HISTORY, NIMBLE_PROPERTY::NOTIFY);
    live_characteristic_ = service->createCharacteristic(UUID_LIVE, NIMBLE_PROPERTY::NOTIFY);
    alerts_characteristic_ = service->createCharacteristic(UUID_ALERTS, NIMBLE_PROPERTY::NOTIFY);
    summary_characteristic_ = service->createCharacteristic(UUID_SUMMARY, NIMBLE_PROPERTY::NOTIFY);

    fill_device_info_();
    service->start();

    refresh_advertising_(true);
    Serial.printf("[BLE] Устройство %s готово, protocol v%u, boot %lu\r\n", device_name_,
                  PROTOCOL_VERSION, static_cast<unsigned long>(boot_counter_));
    return true;
}

void BleManager::update(const CanMetrics &metrics, uint32_t now_ms, uint32_t stale_ms)
{
    lock_();
    int16_t values[METRIC_COUNT] = {};
    const uint8_t presence = ble_metrics_from_can(metrics, now_ms, stale_ms, values);
    memcpy(current_values_, values, sizeof(values));
    current_presence_ = presence;
    // Алерт последнего такта относится к завершающейся поездке и должен попасть
    // в ее сводку до finish_trip_, а не после сохранения summary в NVS
    if (engine_running_ && (presence & (1u << METRIC_RPM)) != 0 && values[METRIC_RPM] == 0) {
        import_alerts_();
    }
    update_trip_(now_ms, values, presence);
    import_alerts_();

    if (engine_running_ && now_ms - last_sample_ms_ >= SAMPLE_INTERVAL_MS) {
        last_sample_ms_ = now_ms;
        StoredTelemetryRecord record = {};
        record.uptime_ms = now_ms;
        memcpy(record.values, values, sizeof(record.values));
        record.trip_seq = trip_seq_;
        accumulate_trip_stats_(values, presence);
        if (history_.append(record)) {
            Serial.println("[BLE] Буфер истории переполнен, старая запись потеряна");
            send_event_(EVENT_BUFFER_OVERFLOW, history_.oldest_seq());
        }
    }
    unlock_();
}

void BleManager::handle()
{
    lock_();
    refresh_advertising_();
    if (!connected_ || !authenticated_) {
        unlock_();
        return;
    }
    const uint32_t now = millis();
    if (live_active_ && static_cast<int32_t>(now - live_next_ms_) >= 0) {
        send_live_();
        live_next_ms_ = now + SAMPLE_INTERVAL_MS;
    }
    if (history_active_) {
        if (history_awaiting_ack_ && now - history_last_send_ms_ >= HISTORY_RETRY_MS) {
            send_history_();
        } else if (!history_awaiting_ack_ && history_batch_ready_(now)) {
            send_history_();
        }
    }
    if (alert_stream_active_) send_alert_();
    if (summary_stream_active_ && summary_pending_index_ < 0) send_summary_();
    unlock_();
}

void BleManager::on_connect()
{
    lock_();
    connected_ = true;
    authenticated_ = false;
    challenge_valid_ = false;
    live_active_ = false;
    history_active_ = false;
    history_awaiting_ack_ = false;
    alert_stream_active_ = false;
    alert_cursor_ = alert_next_index_;
    summary_stream_active_ = false;
    summary_pending_index_ = -1;
    unlock_();
    refresh_advertising_(true);
    Serial.println("[BLE] Телефон подключен");
}

void BleManager::on_disconnect()
{
    lock_();
    connected_ = false;
    authenticated_ = false;
    challenge_valid_ = false;
    live_active_ = false;
    history_active_ = false;
    history_awaiting_ack_ = false;
    alert_stream_active_ = false;
    summary_stream_active_ = false;
    summary_pending_index_ = -1;
    unlock_();
    refresh_advertising_(true);
    Serial.println("[BLE] Телефон отключен, реклама перезапущена");
}

bool BleManager::owner_id_(char out[17]) const
{
    out[0] = '\0';
    if (!paired_) return true;
    static constexpr char LABEL[] = "KANVEX-OWNER-ID-V1";
    uint8_t digest[32] = {};
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info == nullptr || mbedtls_md_hmac(info, token_, sizeof(token_),
        reinterpret_cast<const uint8_t *>(LABEL), sizeof(LABEL) - 1, digest) != 0) return false;
    for (size_t i = 0; i < 8; ++i) snprintf(out + i * 2, 3, "%02x", digest[i]);
    return true;
}

BlePairingStatus BleManager::pairing_status()
{
    lock_();
    BlePairingStatus status;
    memcpy(status.device_name, device_name_, sizeof(device_name_));
    status.paired = paired_;
    status.connected = connected_;
    status.authenticated = authenticated_;
    status.history_capacity = history_.capacity();
    status.history_stored = static_cast<uint16_t>(history_.next_seq() - history_.oldest_seq());
    status.history_oldest_seq = history_.oldest_seq();
    status.history_next_seq = history_.next_seq();
    status.history_acked_upto = history_.acked_upto();
    status.history_lost_unacked = history_.lost_unacked_count();
    const uint32_t unread_from = std::max(history_.acked_upto(), history_.oldest_seq());
    status.history_unread = static_cast<uint16_t>(history_.next_seq() - unread_from);
    status.summaries_stored = summary_count_;
    for (uint8_t i = 0; i < summary_count_; ++i) {
        if (summaries_[i].uploaded == 0) ++status.summaries_unread;
    }
    owner_id_(status.owner_id);
    unlock_();
    return status;
}

BleForgetResult BleManager::forget_phone(const char *expected_owner_id)
{
    lock_();
    if (!paired_) {
        unlock_();
        return BleForgetResult::NOT_PAIRED;
    }
    char owner[17];
    if (!owner_id_(owner) || expected_owner_id == nullptr || strcmp(owner, expected_owner_id) != 0) {
        unlock_();
        return BleForgetResult::STALE_OWNER;
    }
    Preferences preferences;
    if (!preferences.begin("kanvex_ble", false)) {
        unlock_();
        return BleForgetResult::STORAGE_ERROR;
    }
    const bool removed = preferences.remove("token");
    preferences.end();
    if (!removed) {
        unlock_();
        return BleForgetResult::STORAGE_ERROR;
    }
    send_event_(EVENT_TOKENS_CLEARED, 0);
    paired_ = false;
    authenticated_ = false;
    challenge_valid_ = false;
    memset(token_, 0, sizeof(token_));
    memset(nonce_, 0, sizeof(nonce_));
    live_active_ = false;
    history_active_ = false;
    history_awaiting_ack_ = false;
    alert_stream_active_ = false;
    summary_stream_active_ = false;
    summary_pending_index_ = -1;
    fill_device_info_();
    const auto peers = server_->getPeerDevices();
    unlock_();
    // Disconnect вызывает on_disconnect, поэтому здесь мьютекс уже отпущен
    for (const auto peer : peers) server_->disconnect(peer);
    refresh_advertising_(true);
    return BleForgetResult::OK;
}

void BleManager::fill_device_info_()
{
    BleDeviceInfoFrame frame = {};
    frame.protocol_version = PROTOCOL_VERSION;
    frame.frame_format = FRAME_FORMAT_SCHEMA;
    frame.firmware_version = firmware_version_wire();
    frame.boot_counter = boot_counter_;
    frame.uptime_ms = millis();
    frame.trip_seq = trip_seq_;
    frame.flags = (engine_running_ ? BLE_INFO_FLAG_ENGINE_RUNNING : 0) |
                  (time_synced_ ? BLE_INFO_FLAG_TIME_SYNCED : 0) |
                  (summaries_pending_() ? BLE_INFO_FLAG_SUMMARIES_PENDING : 0) |
                  (paired_ ? BLE_INFO_FLAG_PAIRED : 0);
    frame.metric_mask = static_cast<uint16_t>((1u << METRIC_COUNT) - 1u);
    frame.trip_oldest_seq = history_.oldest_seq();
    frame.trip_newest_seq = history_.empty() ? history_.oldest_seq() : history_.newest_seq();
    frame.trip_ring_records = history_.capacity();
    frame.min_protocol_version = PROTOCOL_MIN_VERSION;
    frame.max_protocol_version = PROTOCOL_VERSION;
    frame.capabilities = CAPABILITY_ALL;
    device_info_characteristic_->setValue(reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
}

void BleManager::on_device_info_read()
{
    lock_();
    fill_device_info_();
    unlock_();
}

void BleManager::on_challenge_read()
{
    lock_();
    esp_fill_random(nonce_, sizeof(nonce_));
    challenge_valid_ = true;
    BleAuthChallengeFrame frame = {};
    memcpy(frame.nonce, nonce_, sizeof(frame.nonce));
    frame.boot_counter = boot_counter_;
    challenge_characteristic_->setValue(reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
    unlock_();
}

bool BleManager::command_requires_auth_(uint8_t opcode) const
{
    return opcode != OP_PAIR_CLAIM && opcode != OP_AUTH;
}

void BleManager::on_control_write(const uint8_t *data, size_t length)
{
    if (data == nullptr || length == 0 || length > PROTOCOL_CONTROL_MAX_LEN) return;
    lock_();
    const uint8_t opcode = data[0];
    if (command_requires_auth_(opcode) && !authenticated_) {
        send_status_(opcode, RESULT_AUTH_REQUIRED);
        unlock_();
        return;
    }

    switch (opcode) {
        case OP_PAIR_CLAIM:
            if (length != 1 + PROTOCOL_TOKEN_SIZE) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else if (paired_) {
                send_status_(opcode, RESULT_ALREADY_PAIRED);
            } else if (!save_token(data + 1)) {
                send_status_(opcode, RESULT_BUSY);
            } else {
                memcpy(token_, data + 1, PROTOCOL_TOKEN_SIZE);
                paired_ = true;
                authenticated_ = true;
                send_status_(opcode, RESULT_OK);
            }
            break;
        case OP_AUTH:
            if (length != 1 + PROTOCOL_AUTH_TAG_SIZE) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else if (!challenge_valid_) {
                send_status_(opcode, RESULT_CHALLENGE_REQUIRED);
            } else {
                send_status_(opcode, verify_auth_(data + 1) ? RESULT_OK : RESULT_UNKNOWN_TOKEN);
            }
            break;
        case OP_TIME_SYNC:
            if (length != 9) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else {
                memcpy(&time_unix_ref_, data + 1, sizeof(time_unix_ref_));
                time_uptime_ref_ = millis();
                const bool saved = save_time_anchor(time_unix_ref_, time_uptime_ref_, boot_counter_);
                if (saved) time_synced_ = true;
                send_status_(opcode, saved ? RESULT_OK : RESULT_BUSY);
            }
            break;
        case OP_SUBSCRIBE_HISTORY: {
            if (length != 7 || data[1] != BLE_HISTORY_LEVEL_TRIP || (data[2] & ~BLE_SUBSCRIBE_FLAG_FOLLOW)) {
                send_status_(opcode, RESULT_BAD_ARGS);
                break;
            }
            uint32_t from_seq = 0;
            memcpy(&from_seq, data + 3, sizeof(from_seq));
            history_follow_ = (data[2] & BLE_SUBSCRIBE_FLAG_FOLLOW) != 0;
            history_next_seq_ = from_seq == BLE_FROM_SEQ_ONLY_NEW ? history_.next_seq() : from_seq;
            history_active_ = true;
            history_awaiting_ack_ = false;
            history_gap_before_ = false;
            history_catching_up_ = history_next_seq_ < history_.next_seq();
            history_last_send_ms_ = millis();
            if (history_next_seq_ < history_.oldest_seq()) {
                const uint32_t oldest = history_.oldest_seq();
                history_gap_before_ = true;
                send_status_(opcode, RESULT_RECORDS_ERASED, oldest << 8);
            } else {
                send_status_(opcode, RESULT_OK);
            }
            break;
        }
        case OP_ACK_HISTORY: {
            if (length != 6 || data[1] != BLE_HISTORY_LEVEL_TRIP) {
                send_status_(opcode, RESULT_BAD_ARGS);
                break;
            }
            uint32_t acked_seq = 0;
            memcpy(&acked_seq, data + 2, sizeof(acked_seq));
            if (acked_seq < history_next_seq_ && acked_seq + 1 == history_next_seq_) {
                // Повтор ACK после принятого ответа безопасен и не сбивает текущую пачку
                send_status_(opcode, RESULT_OK);
            } else if (!history_awaiting_ack_ || acked_seq != history_pending_seq_) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else {
                history_.ack(acked_seq);
                history_next_seq_ = acked_seq + 1;
                history_awaiting_ack_ = false;
                history_gap_before_ = false;
                if (history_pending_caught_up_) history_catching_up_ = false;
                if (!history_follow_ && history_next_seq_ >= history_.next_seq()) history_active_ = false;
                send_status_(opcode, RESULT_OK);
            }
            break;
        }
        case OP_UNSUBSCRIBE_HISTORY:
            if (length != 2 || data[1] != BLE_HISTORY_LEVEL_TRIP) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else {
                history_active_ = false;
                history_awaiting_ack_ = false;
                send_status_(opcode, RESULT_OK);
            }
            break;
        case OP_LIVE_START:
            if (length != 2 || data[1] != LIVE_PERIOD_25MS) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else {
                live_active_ = true;
                live_next_ms_ = millis();
                send_status_(opcode, RESULT_OK);
            }
            break;
        case OP_LIVE_STOP:
            if (length != 1) send_status_(opcode, RESULT_BAD_ARGS);
            else {
                live_active_ = false;
                send_status_(opcode, RESULT_OK);
            }
            break;
        case OP_FETCH_SUMMARIES:
            if (length != 2 || (data[1] & ~1u)) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else {
                summary_cursor_ = 0;
                summary_pending_index_ = -1;
                summary_pending_only_ = (data[1] & 1u) != 0;
                summary_stream_active_ = true;
                send_status_(opcode, RESULT_OK);
            }
            break;
        case OP_ACK_SUMMARY: {
            if (length != 6 || summary_pending_index_ < 0) {
                send_status_(opcode, RESULT_BAD_ARGS);
                break;
            }
            uint32_t boot = 0;
            memcpy(&boot, data + 1, sizeof(boot));
            StoredSummary &pending = summaries_[summary_pending_index_];
            if (pending.frame.boot_counter != boot || pending.frame.trip_seq != data[5]) {
                send_status_(opcode, RESULT_BAD_ARGS);
            } else {
                pending.uploaded = 1;
                summary_cursor_ = static_cast<uint8_t>(summary_pending_index_ + 1);
                summary_pending_index_ = -1;
                save_summaries_();
                send_status_(opcode, RESULT_OK);
            }
            break;
        }
        case OP_FETCH_ALERTS: {
            if (length != 5) {
                send_status_(opcode, RESULT_BAD_ARGS);
                break;
            }
            uint32_t from_index = 0;
            memcpy(&from_index, data + 1, sizeof(from_index));
            const uint32_t oldest = alert_next_index_ > ALERT_RING_SIZE
                ? alert_next_index_ - ALERT_RING_SIZE : 0;
            alert_cursor_ = from_index < oldest ? oldest : from_index;
            alert_stream_active_ = true;
            send_status_(opcode, RESULT_OK);
            break;
        }
        default:
            send_status_(opcode, RESULT_UNKNOWN_COMMAND);
            break;
    }
    unlock_();
}

bool BleManager::verify_auth_(const uint8_t *tag)
{
    challenge_valid_ = false;
    if (!paired_) return false;
    uint8_t message[sizeof(AUTH_PREFIX) - 1 + PROTOCOL_AUTH_NONCE_SIZE + sizeof(boot_counter_)] = {};
    memcpy(message, AUTH_PREFIX, sizeof(AUTH_PREFIX) - 1);
    memcpy(message + sizeof(AUTH_PREFIX) - 1, nonce_, sizeof(nonce_));
    memcpy(message + sizeof(AUTH_PREFIX) - 1 + sizeof(nonce_), &boot_counter_, sizeof(boot_counter_));

    uint8_t digest[32] = {};
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info == nullptr || mbedtls_md_hmac(info, token_, sizeof(token_), message, sizeof(message), digest) != 0) {
        return false;
    }
    uint8_t difference = 0;
    for (size_t i = 0; i < PROTOCOL_AUTH_TAG_SIZE; ++i) difference |= digest[i] ^ tag[i];
    authenticated_ = difference == 0;
    return authenticated_;
}

void BleManager::send_status_(uint8_t opcode, uint8_t result, uint32_t extra)
{
    BleStatusFrame frame = {};
    frame.kind = BLE_STATUS_KIND_REPLY;
    frame.code = opcode;
    frame.detail = extra | result;
    status_characteristic_->setValue(reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
    status_characteristic_->notify();
}

void BleManager::send_event_(uint8_t event, uint32_t detail)
{
    if (!connected_ || !authenticated_) return;
    BleStatusFrame frame = {};
    frame.kind = BLE_STATUS_KIND_EVENT;
    frame.code = event;
    frame.detail = detail;
    status_characteristic_->setValue(reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
    status_characteristic_->notify();
}

void BleManager::send_live_()
{
    uint8_t data[PROTOCOL_SIZE_LIVE_HEADER + 1 + METRIC_COUNT * sizeof(int16_t)] = {};
    BleLiveHeader header = {};
    header.uptime_ms = millis();
    header.boot_counter = boot_counter_;
    header.trip_seq = trip_seq_;
    header.flags = engine_running_ ? BLE_LIVE_FLAG_ENGINE_RUNNING : 0;
    const uint32_t schema_id = ble_metric_schema_id();
    header.schema_id[0] = static_cast<uint8_t>(schema_id);
    header.schema_id[1] = static_cast<uint8_t>(schema_id >> 8);
    header.schema_id[2] = static_cast<uint8_t>(schema_id >> 16);
    memcpy(data, &header, sizeof(header));
    ble_encode_metric_values(current_values_, current_presence_,
                             data + sizeof(header), sizeof(data) - sizeof(header));
    live_characteristic_->setValue(data, sizeof(data));
    live_characteristic_->notify();
}

void BleManager::send_history_()
{
    if (history_next_seq_ < history_.oldest_seq()) {
        history_next_seq_ = history_.oldest_seq();
        history_gap_before_ = true;
        // Уже отправленная пачка могла целиком исчезнуть до ACK
        // В этом случае формируем новую, иначе end_seq останется позади курсора
        if (history_awaiting_ack_ && history_pending_seq_ < history_next_seq_) {
            history_awaiting_ack_ = false;
            history_catching_up_ = true;
        }
    }
    StoredTelemetryRecord first_record = {};
    if (!history_.get(history_next_seq_, first_record)) return;

    static constexpr size_t RECORD_SIZE = 2 + 1 + METRIC_COUNT * sizeof(int16_t);
    const uint8_t record_capacity = history_record_capacity_();
    if (record_capacity == 0) return;

    uint8_t data[PROTOCOL_SIZE_HISTORY_HEADER + 26 * RECORD_SIZE] = {};
    BleHistoryHeader header = {};
    header.level = BLE_HISTORY_LEVEL_TRIP;
    header.frame_format = FRAME_FORMAT_SCHEMA;
    if (history_gap_before_) header.flags |= BLE_HISTORY_FLAG_GAP_BEFORE;
    header.first_seq = history_next_seq_;
    header.first_uptime_ms = first_record.uptime_ms;
    header.boot_counter = boot_counter_;
    header.trip_seq = trip_seq_for_record_(history_next_seq_);
    const uint32_t schema_id = ble_metric_schema_id();
    header.schema_id[0] = static_cast<uint8_t>(schema_id);
    header.schema_id[1] = static_cast<uint8_t>(schema_id >> 8);
    header.schema_id[2] = static_cast<uint8_t>(schema_id >> 16);
    size_t encoded_size = sizeof(header);
    const uint32_t end_seq = history_awaiting_ack_
        ? history_pending_seq_ + 1 : history_.next_seq();
    uint32_t seq = history_next_seq_;
    uint32_t previous_uptime_ms = first_record.uptime_ms;
    for (; header.count < record_capacity && seq < end_seq; ++seq) {
        StoredTelemetryRecord record = {};
        if (!history_.get(seq, record) || trip_seq_for_record_(seq) != header.trip_seq) break;
        const uint32_t delta = header.count == 0 ? 0 : record.uptime_ms - previous_uptime_ms;
        if (delta > UINT16_MAX) break;
        const uint16_t delta_ms = static_cast<uint16_t>(delta);
        memcpy(data + encoded_size, &delta_ms, sizeof(delta_ms));
        encoded_size += sizeof(delta_ms);
        uint8_t presence = 0;
        for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
            if (record.values[i] != static_cast<int16_t>(PROTOCOL_NO_VALUE_SIGNED)) presence |= 1 << i;
        }
        ble_encode_metric_values(record.values, presence, data + encoded_size,
                                 sizeof(data) - encoded_size);
        encoded_size += ble_metric_values_size();
        previous_uptime_ms = record.uptime_ms;
        ++header.count;
    }
    if (header.count == 0) return;
    if (!history_awaiting_ack_) {
        history_pending_seq_ = seq - 1;
        history_pending_caught_up_ = history_pending_seq_ + 1 >= history_.next_seq();
    }
    if (history_pending_caught_up_) header.flags |= BLE_HISTORY_FLAG_CAUGHT_UP;
    memcpy(data, &header, sizeof(header));
    history_characteristic_->setValue(data, encoded_size);
    history_characteristic_->notify();
    history_awaiting_ack_ = true;
    history_last_send_ms_ = millis();
}

uint8_t BleManager::history_record_capacity_()
{
    static constexpr size_t RECORD_SIZE = 2 + 1 + METRIC_COUNT * sizeof(int16_t);
    uint16_t mtu = 23;
    const std::vector<uint16_t> peers = server_->getPeerDevices();
    if (!peers.empty()) mtu = server_->getPeerMTU(peers[0]);
    const size_t payload = mtu > 3 ? mtu - 3 : 0;
    size_t count = payload > PROTOCOL_SIZE_HISTORY_HEADER
        ? (payload - PROTOCOL_SIZE_HISTORY_HEADER) / RECORD_SIZE : 0;
    if (count > 26) count = 26;
    return static_cast<uint8_t>(count);
}

bool BleManager::history_batch_ready_(uint32_t now_ms)
{
    if (history_next_seq_ < history_.oldest_seq()) return true;
    if (history_next_seq_ >= history_.next_seq()) return false;
    const uint8_t capacity = history_record_capacity_();
    if (capacity == 0) return false;
    const uint8_t first_trip = trip_seq_for_record_(history_next_seq_);
    uint8_t count = 0;
    for (uint32_t seq = history_next_seq_; seq < history_.next_seq() && count < capacity; ++seq) {
        if (trip_seq_for_record_(seq) != first_trip) return true;
        ++count;
    }
    return history_catching_up_ || count >= capacity || !engine_running_ ||
           now_ms - history_last_send_ms_ >= HISTORY_FLUSH_MS;
}

void BleManager::send_alert_()
{
    const uint32_t oldest = alert_next_index_ > ALERT_RING_SIZE
        ? alert_next_index_ - ALERT_RING_SIZE : 0;
    if (alert_cursor_ < oldest) alert_cursor_ = oldest;
    if (alert_cursor_ >= alert_next_index_) {
        alert_stream_active_ = false;
        return;
    }
    const BleAlertFrame &frame = alert_ring_[alert_cursor_ % ALERT_RING_SIZE].frame;
    if (frame.index != alert_cursor_) {
        alert_cursor_ = oldest;
        return;
    }
    alerts_characteristic_->setValue(
        reinterpret_cast<const uint8_t *>(&frame), sizeof(frame));
    alerts_characteristic_->notify();
    ++alert_cursor_;
}

void BleManager::send_summary_()
{
    while (summary_cursor_ < summary_count_ &&
           summary_pending_only_ && summaries_[summary_cursor_].uploaded != 0) {
        ++summary_cursor_;
    }
    if (summary_cursor_ >= summary_count_) {
        summary_stream_active_ = false;
        return;
    }

    const BleSummaryFrame &frame = summaries_[summary_cursor_].frame;
    uint8_t data[PROTOCOL_SIZE_SUMMARY_HEADER + METRIC_COUNT * 3 * sizeof(int16_t)] = {};
    memcpy(data, &frame.boot_counter, sizeof(frame.boot_counter));
    data[4] = frame.trip_seq;
    data[5] = frame.flags;
    memcpy(data + 6, &frame.alert_mask, sizeof(frame.alert_mask));
    memcpy(data + 8, &frame.start_unix_ms, sizeof(frame.start_unix_ms));
    memcpy(data + 16, &frame.duration_s, sizeof(frame.duration_s));
    const uint32_t schema_id = ble_metric_schema_id();
    data[20] = static_cast<uint8_t>(schema_id);
    data[21] = static_cast<uint8_t>(schema_id >> 8);
    data[22] = static_cast<uint8_t>(schema_id >> 16);
    data[23] = METRIC_COUNT;
    memcpy(data + PROTOCOL_SIZE_SUMMARY_HEADER, frame.stats, sizeof(frame.stats));
    summary_characteristic_->setValue(data, sizeof(data));
    summary_characteristic_->notify();
    summary_pending_index_ = summary_cursor_;
}

void BleManager::refresh_advertising_(bool force)
{
    const uint32_t now = millis();
    if (!force && now - advertising_refresh_ms_ < ADV_REFRESH_MS) return;

    uint8_t flags = (engine_running_ ? ADV_FLAG_ENGINE_RUNNING : 0) |
                    (history_.acked_upto() < history_.next_seq() ? ADV_FLAG_UNACKED_RECORDS : 0) |
                    (summaries_pending_() ? ADV_FLAG_SUMMARIES_PENDING : 0) |
                    (paired_ ? ADV_FLAG_PAIRED : 0);
    advertising_refresh_ms_ = now;
    if (!force && flags == last_advertising_flags_ && trip_seq_ == last_advertising_trip_seq_) return;

    BleAdvStatusData status = {};
    status.company_id = static_cast<uint16_t>(PROTOCOL_COMPANY_ID_TEST);
    status.status_version = static_cast<uint8_t>(PROTOCOL_STATUS_VERSION);
    status.flags = flags;
    status.boot_counter = boot_counter_;
    status.trip_seq = trip_seq_;

    NimBLEAdvertisementData advertisement_data;
    advertisement_data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advertisement_data.setCompleteServices(NimBLEUUID(UUID_SERVICE));
    NimBLEAdvertisementData scan_response;
    scan_response.setName(device_name_);
    scan_response.setManufacturerData(std::vector<uint8_t>(
        reinterpret_cast<uint8_t *>(&status),
        reinterpret_cast<uint8_t *>(&status) + sizeof(status)));

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    if (advertising->isAdvertising()) advertising->stop();
    advertising->setAdvertisementData(advertisement_data);
    advertising->setScanResponseData(scan_response);
    advertising->setScanResponse(true);
    advertising->setMinInterval(ADV_INTERVAL_UNITS);
    advertising->setMaxInterval(ADV_INTERVAL_UNITS);
    advertising->setAdvertisementType(connected_ ? BLE_GAP_CONN_MODE_NON : BLE_GAP_CONN_MODE_UND);
    advertising->start();
    last_advertising_flags_ = flags;
    last_advertising_trip_seq_ = trip_seq_;
}

void BleManager::update_trip_(uint32_t now_ms, const int16_t values[METRIC_COUNT], uint8_t presence)
{
    const uint8_t rpm_bit = static_cast<uint8_t>(1u << METRIC_RPM);
    if ((presence & rpm_bit) == 0) return;
    if (values[METRIC_RPM] != 0) {
        if (!engine_running_) start_trip_(now_ms);
    } else if (engine_running_) {
        finish_trip_(now_ms);
    }
}

void BleManager::start_trip_(uint32_t now_ms)
{
    if (!first_trip_) ++trip_seq_;
    first_trip_ = false;
    engine_running_ = true;
    trip_start_ms_ = now_ms;
    last_sample_ms_ = now_ms - SAMPLE_INTERVAL_MS;
    trip_alert_mask_ = 0;
    reset_trip_stats_();

    send_event_(EVENT_TRIP_STARTED, trip_seq_);
}

void BleManager::finish_trip_(uint32_t now_ms)
{
    BleSummaryFrame summary = {};
    summary.boot_counter = boot_counter_;
    summary.trip_seq = trip_seq_;
    summary.alert_mask = trip_alert_mask_;
    summary.duration_s = (now_ms - trip_start_ms_) / 1000u;
    if (time_synced_) {
        summary.flags |= SUMMARY_FLAG_TIME_KNOWN;
        const int32_t delta = static_cast<int32_t>(trip_start_ms_ - time_uptime_ref_);
        summary.start_unix_ms = delta >= 0
            ? time_unix_ref_ + static_cast<uint32_t>(delta)
            : time_unix_ref_ - static_cast<uint32_t>(-static_cast<int64_t>(delta));
    }
    for (uint8_t metric = 0; metric < METRIC_COUNT; ++metric) {
        if (trip_count_[metric] == 0) {
            summary.stats[metric][0] = static_cast<int16_t>(PROTOCOL_NO_VALUE_SIGNED);
            summary.stats[metric][1] = static_cast<int16_t>(PROTOCOL_NO_VALUE_SIGNED);
            summary.stats[metric][2] = static_cast<int16_t>(PROTOCOL_NO_VALUE_SIGNED);
        } else {
            summary.stats[metric][0] = static_cast<int16_t>(trip_min_[metric]);
            summary.stats[metric][1] = static_cast<int16_t>(trip_max_[metric]);
            summary.stats[metric][2] = metric_average(trip_sum_[metric], trip_count_[metric]);
        }
    }

    if (summary_count_ == SUMMARY_CAPACITY) {
        memmove(summaries_, summaries_ + 1,
                sizeof(summaries_[0]) * (SUMMARY_CAPACITY - 1));
        --summary_count_;
    }
    summaries_[summary_count_].frame = summary;
    summaries_[summary_count_].uploaded = 0;
    ++summary_count_;
    save_summaries_();
    engine_running_ = false;
    send_event_(EVENT_TRIP_FINISHED, trip_seq_);
    send_event_(EVENT_SUMMARY_READY, boot_counter_);
}

void BleManager::reset_trip_stats_()
{
    for (uint8_t metric = 0; metric < METRIC_COUNT; ++metric) {
        trip_min_[metric] = INT16_MAX;
        trip_max_[metric] = INT16_MIN;
        trip_sum_[metric] = 0;
        trip_count_[metric] = 0;
    }
}

void BleManager::accumulate_trip_stats_(const int16_t values[METRIC_COUNT], uint8_t presence)
{
    for (uint8_t metric = 0; metric < METRIC_COUNT; ++metric) {
        if ((presence & (1u << metric)) == 0) continue;
        trip_min_[metric] = std::min(trip_min_[metric], static_cast<int32_t>(values[metric]));
        trip_max_[metric] = std::max(trip_max_[metric], static_cast<int32_t>(values[metric]));
        trip_sum_[metric] += values[metric];
        ++trip_count_[metric];
    }
}

uint8_t BleManager::trip_seq_for_record_(uint32_t seq) const
{
    StoredTelemetryRecord record = {};
    return history_.get(seq, record) ? record.trip_seq : trip_seq_;
}

void BleManager::import_alerts_()
{
    const uint32_t oldest = alert_manager.event_oldest_index();
    if (observed_alert_index_ < oldest) observed_alert_index_ = oldest;
    AlertTransition transition = {};
    while (alert_manager.event_at(observed_alert_index_, transition)) {
        BleAlertFrame frame = {};
        frame.index = alert_next_index_;
        frame.uptime_ms = transition.uptime_ms;
        frame.boot_counter = boot_counter_;
        frame.trip_seq = trip_seq_;
        frame.code = transition.code;
        frame.state = transition.raised ? 1 : 0;
        frame.metric_id = transition.metric_id;
        frame.value = transition.value;
        alert_ring_[alert_next_index_ % ALERT_RING_SIZE].frame = frame;
        ++alert_next_index_;
        ++observed_alert_index_;
        if (transition.raised && engine_running_ && transition.code >= 1 && transition.code <= 16) {
            trip_alert_mask_ |= static_cast<uint16_t>(1u << (transition.code - 1));
        }
        if (connected_ && authenticated_) alert_stream_active_ = true;
    }
}

bool BleManager::load_summaries_()
{
    Preferences preferences;
    if (!preferences.begin("kanvex_ble", true)) return false;
    const uint8_t count = preferences.getUChar("sum_count", 0);
    const size_t expected = static_cast<size_t>(count) * sizeof(StoredSummary);
    const bool valid = count <= SUMMARY_CAPACITY &&
                       preferences.getBytesLength("summaries") == expected;
    if (valid && expected != 0) {
        preferences.getBytes("summaries", summaries_, expected);
    }
    preferences.end();
    summary_count_ = valid ? count : 0;
    if (!valid) Serial.println("[BLE] Хранилище сводок повреждено, оно сброшено в RAM");
    return valid;
}

bool BleManager::save_summaries_()
{
    Preferences preferences;
    if (!preferences.begin("kanvex_ble", false)) return false;
    bool saved = preferences.putUChar("sum_count", summary_count_) == sizeof(summary_count_);
    if (summary_count_ != 0) {
        const size_t size = static_cast<size_t>(summary_count_) * sizeof(StoredSummary);
        saved = preferences.putBytes("summaries", summaries_, size) == size && saved;
    }
    preferences.end();
    if (!saved) Serial.println("[BLE] Не удалось сохранить сводки поездок");
    return saved;
}

bool BleManager::summaries_pending_() const
{
    for (uint8_t i = 0; i < summary_count_; ++i) {
        if (summaries_[i].uploaded == 0) return true;
    }
    return false;
}

void BleManager::lock_()
{
    if (mutex_ != nullptr) xSemaphoreTake(mutex_, portMAX_DELAY);
}

void BleManager::unlock_()
{
    if (mutex_ != nullptr) xSemaphoreGive(mutex_);
}

