#include <unity.h>

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <openssl/hmac.h>

#include "AlertManager.h"
#include "BleManager.h"
#include "BuzzerStub.h"

static std::map<std::string, std::vector<uint8_t>> s_fixtures;

static std::string uuid(uint8_t entity)
{
    char value[64];
    snprintf(value, sizeof(value), "f0a0b1%02x-9c4e-4b7a-9d1e-2f6c8a3b0001", entity);
    return value;
}

static NimBLECharacteristic &characteristic(uint8_t entity)
{
    return *NimBLEDevice::server.service.characteristics.at(uuid(entity));
}

static uint32_t u32(const std::vector<uint8_t> &bytes, size_t offset)
{
    uint32_t value = 0;
    memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

static std::vector<uint8_t> number(uint64_t value, size_t size)
{
    std::vector<uint8_t> bytes(size);
    for (size_t i = 0; i < size; ++i) bytes[i] = static_cast<uint8_t>(value >> (8 * i));
    return bytes;
}

static void command(uint8_t opcode, const std::vector<uint8_t> &body = {})
{
    std::vector<uint8_t> bytes = {opcode};
    bytes.insert(bytes.end(), body.begin(), body.end());
    characteristic(KANVEX_UUID_ENTITY_CONTROL).write(bytes);
}

static void result(uint8_t opcode, uint8_t expected)
{
    const auto &bytes = characteristic(KANVEX_UUID_ENTITY_STATUS).value;
    TEST_ASSERT_EQUAL_UINT(8, bytes.size());
    TEST_ASSERT_EQUAL_UINT8(0, bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(opcode, bytes[1]);
    TEST_ASSERT_EQUAL_UINT8(expected, bytes[4]);
}

static void reset()
{
    mock_nvs.clear();
    mock_nvs["boot"] = number(6, 4);
    mock_nvs_write_failed = false;
    mock_notifications.clear();
    mock_mtu = 185;
    mock_set_millis(1000);
    mock_fs_reset();
    alert_manager = AlertManager();
    TEST_ASSERT_TRUE(alert_manager.init());
}

static std::vector<uint8_t> token()
{
    std::vector<uint8_t> value(16);
    for (size_t i = 0; i < value.size(); ++i) value[i] = static_cast<uint8_t>(i);
    return value;
}

static void claim(BleManager &manager)
{
    manager.on_connect();
    command(OP_PAIR_CLAIM, token());
    result(OP_PAIR_CLAIM, RESULT_OK);
}

static std::vector<uint8_t> proof(const std::vector<uint8_t> &challenge)
{
    const std::string prefix = "KANVEX-AUTH-V6";
    std::vector<uint8_t> message(prefix.begin(), prefix.end());
    message.insert(message.end(), challenge.begin(), challenge.end());
    unsigned char digest[32];
    unsigned int size = 0;
    const auto key = token();
    HMAC(EVP_sha256(), key.data(), key.size(), message.data(), message.size(), digest, &size);
    return std::vector<uint8_t>(digest, digest + PROTOCOL_AUTH_TAG_SIZE);
}

static void sample(BleManager &manager, uint32_t at, float rpm = 1750)
{
    mock_set_millis(at);
    CanMetrics metrics = {};
    metrics.engine_rpm = rpm;
    metrics.engine_rpm_ts = at;
    metrics.engine_coolant = 91.2f;
    metrics.engine_coolant_ts = at;
    manager.update(metrics, at, 500);
}

static void subscribe(uint32_t from)
{
    auto body = std::vector<uint8_t>{0, 1};
    const auto seq = number(from, 4);
    body.insert(body.end(), seq.begin(), seq.end());
    command(OP_OPEN_HISTORY, body);
    result(OP_OPEN_HISTORY, RESULT_OK);
    const auto state = characteristic(KANVEX_UUID_ENTITY_HISTORY_STATE).read();
    TEST_ASSERT_EQUAL_UINT(PROTOCOL_SIZE_HISTORY_STATE, state.size());
    TEST_ASSERT_EQUAL_UINT32(7, u32(state, 4));
    command(OP_START_HISTORY, number(u32(state, 0), 4));
    result(OP_START_HISTORY, RESULT_OK);
}

static void ack(uint32_t seq)
{
    auto body = number(u32(characteristic(KANVEX_UUID_ENTITY_HISTORY_STATE).value, 0), 4);
    const auto bytes = number(seq, 4);
    body.insert(body.end(), bytes.begin(), bytes.end());
    command(OP_ACK_HISTORY, body);
}

void test_qx50m_discovery_and_auth_contract()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    TEST_ASSERT_EQUAL_STRING_LEN("QX50M_", manager.device_name(), 6);
    const auto &advertising = NimBLEDevice::advertising;
    TEST_ASSERT_EQUAL_STRING(uuid(KANVEX_UUID_ENTITY_SERVICE).c_str(), advertising.advertisement.service.c_str());
    TEST_ASSERT_EQUAL_STRING(manager.device_name(), advertising.response.name.c_str());
    TEST_ASSERT_EQUAL_UINT(9, advertising.response.manufacturer.size());
    TEST_ASSERT_EQUAL_HEX8(0xFF, advertising.response.manufacturer[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, advertising.response.manufacturer[1]);
    TEST_ASSERT_EQUAL_UINT8(PROTOCOL_STATUS_VERSION, advertising.response.manufacturer[2]);
    TEST_ASSERT_EQUAL_UINT(11, NimBLEDevice::server.service.characteristics.size());
    const auto rules = characteristic(KANVEX_UUID_ENTITY_METRIC_COLOR_RULES).read();
    TEST_ASSERT_EQUAL_UINT(74, rules.size());
    TEST_ASSERT_EQUAL_UINT8(1, rules[0]);
    TEST_ASSERT_EQUAL_UINT8(METRIC_COUNT, rules[1]);
    TEST_ASSERT_EQUAL_UINT(NIMBLE_PROPERTY::WRITE, characteristic(KANVEX_UUID_ENTITY_CONTROL).properties);
    TEST_ASSERT_EQUAL_UINT(NIMBLE_PROPERTY::NOTIFY, characteristic(KANVEX_UUID_ENTITY_HISTORY).properties);
    manager.on_connect();
    command(OP_TIME_SYNC, number(1800000000000ULL, 8));
    result(OP_TIME_SYNC, RESULT_AUTH_REQUIRED);
    claim(manager);
    command(OP_PAIR_CLAIM, token());
    result(OP_PAIR_CLAIM, RESULT_ALREADY_PAIRED);
    manager.on_disconnect();
    manager.on_connect();
    const auto fresh = characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read();
    auto wrong = proof(fresh);
    wrong[0] ^= 1;
    command(OP_AUTH, wrong);
    result(OP_AUTH, RESULT_UNKNOWN_TOKEN);
    command(OP_TIME_SYNC, number(1800000000000ULL, 8));
    result(OP_TIME_SYNC, RESULT_AUTH_REQUIRED);
    characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read();
    command(OP_AUTH, proof(fresh));
    result(OP_AUTH, RESULT_OK);
    command(OP_AUTH, token());
    result(OP_AUTH, RESULT_CHALLENGE_REQUIRED);
    const auto challenge = characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read();
    const auto tag = proof(challenge);
    const uint8_t golden[] = {0x73, 0x9a, 0xa6, 0x3d, 0xf6, 0x56, 0xa0, 0x6d,
                              0x5c, 0xa9, 0x67, 0x0c, 0xc7, 0x98, 0x1d, 0x21};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden, tag.data(), sizeof(golden));
    command(OP_AUTH, tag);
    result(OP_AUTH, RESULT_OK);
    command(OP_AUTH, tag);
    result(OP_AUTH, RESULT_CHALLENGE_REQUIRED);
}

void test_pair_claim_does_not_succeed_if_nvs_write_fails()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    manager.on_connect();
    mock_nvs_write_failed = true;
    command(OP_PAIR_CLAIM, token());
    result(OP_PAIR_CLAIM, RESULT_BUSY);
    TEST_ASSERT_TRUE(mock_nvs["token"].empty());
}

void test_history_mtu_and_exact_retry_before_ack()
{
    for (const uint16_t mtu : {185, 256, 517}) {
        reset();
        BleManager manager;
        TEST_ASSERT_TRUE(manager.begin());
        claim(manager);
        mock_mtu = mtu;
        for (uint32_t i = 1; i <= 40; ++i) sample(manager, i * 1000);
        subscribe(0);
        result(OP_START_HISTORY, RESULT_OK);
        manager.handle();
        const auto first = characteristic(KANVEX_UUID_ENTITY_HISTORY).value;
        const uint8_t capacity = (mtu - 3 - PROTOCOL_SIZE_HISTORY_HEADER) / 19;
        TEST_ASSERT_EQUAL_UINT8(capacity, first[2]);
        TEST_ASSERT_LESS_OR_EQUAL_UINT(mtu - 3, first.size());
        ack(capacity);
        result(OP_ACK_HISTORY, RESULT_ACK_MISMATCH);
        mock_advance_millis(5000);
        manager.handle();
        TEST_ASSERT_TRUE(first == characteristic(KANVEX_UUID_ENTITY_HISTORY).value);
        ack(capacity - 1);
        result(OP_ACK_HISTORY, RESULT_OK);
        ack(capacity - 1);
        result(OP_ACK_HISTORY, RESULT_OK);
        manager.handle();
        TEST_ASSERT_EQUAL_UINT32(capacity, u32(characteristic(KANVEX_UUID_ENTITY_HISTORY).value, 4));
        ack(capacity - 1);
        result(OP_ACK_HISTORY, RESULT_OK);
        const auto state = manager.pairing_status();
        TEST_ASSERT_EQUAL_UINT16(1800, state.history_capacity);
        TEST_ASSERT_EQUAL_UINT16(40, state.history_stored);
        TEST_ASSERT_EQUAL_UINT16(40 - capacity, state.history_unread);
        TEST_ASSERT_EQUAL_UINT32(capacity, state.history_acked_upto);
    }
}

void test_history_overwrite_while_waiting_for_ack_recovers_with_gap()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    claim(manager);
    sample(manager, 1000);
    subscribe(0);
    manager.handle();
    const auto first = characteristic(KANVEX_UUID_ENTITY_HISTORY).value;
    TEST_ASSERT_EQUAL_UINT32(0, u32(characteristic(KANVEX_UUID_ENTITY_HISTORY).value, 4));
    for (uint32_t i = 2; i <= 1810; ++i) sample(manager, i * 1000);
    manager.handle();
    TEST_ASSERT_TRUE(first == characteristic(KANVEX_UUID_ENTITY_HISTORY).value);
    ack(0);
    result(OP_ACK_HISTORY, RESULT_OK);
    manager.handle();
    const auto &batch = characteristic(KANVEX_UUID_ENTITY_HISTORY).value;
    TEST_ASSERT_EQUAL_UINT32(10, u32(batch, 4));
    TEST_ASSERT_TRUE((batch[3] & BLE_HISTORY_FLAG_GAP_BEFORE) != 0);
    ack(10 + batch[2] - 1);
    result(OP_ACK_HISTORY, RESULT_OK);
}

void test_backlog_drains_after_reconnect_from_committed_cursor()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    claim(manager);
    mock_mtu = 185;
    for (uint32_t i = 1; i <= 300; ++i) sample(manager, i * 1000);
    subscribe(0);
    uint32_t next = 0;
    uint32_t batches = 0;
    while (next < 300) {
        manager.handle();
        const auto &batch = characteristic(KANVEX_UUID_ENTITY_HISTORY).value;
        TEST_ASSERT_EQUAL_UINT32(next, u32(batch, 4));
        TEST_ASSERT_GREATER_THAN_UINT8(0, batch[2]);
        next += batch[2];
        ack(next - 1);
        result(OP_ACK_HISTORY, RESULT_OK);
        ++batches;
        if (batches == 10) {
            manager.on_disconnect();
            manager.on_connect();
            command(OP_AUTH, proof(characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read()));
            result(OP_AUTH, RESULT_OK);
            subscribe(next);
            const auto state = characteristic(KANVEX_UUID_ENTITY_HISTORY_STATE).read();
            TEST_ASSERT_EQUAL_UINT32(300 - next, u32(state, 20));
        }
        TEST_ASSERT_LESS_THAN_UINT32(100, batches);
    }
    TEST_ASSERT_EQUAL_UINT32(300, next);
    TEST_ASSERT_EQUAL_UINT32(300, manager.pairing_status().history_acked_upto);
}

void test_unacknowledged_summary_survives_controller_reboot()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    claim(manager);
    command(OP_TIME_SYNC, number(1800000000000ULL, 8));
    sample(manager, 1000);
    sample(manager, 2000);
    sample(manager, 3000, 0);
    command(OP_FETCH_SUMMARIES, {1});
    manager.handle();
    const auto before = characteristic(KANVEX_UUID_ENTITY_SUMMARY).value;
    manager.on_disconnect();
    BleManager rebooted;
    TEST_ASSERT_TRUE(rebooted.begin());
    rebooted.on_connect();
    command(OP_AUTH, proof(characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read()));
    result(OP_AUTH, RESULT_OK);
    command(OP_FETCH_SUMMARIES, {1});
    rebooted.handle();
    TEST_ASSERT_TRUE(before == characteristic(KANVEX_UUID_ENTITY_SUMMARY).value);
    auto wrong_boot = number(8, 4);
    wrong_boot.push_back(0);
    command(OP_ACK_SUMMARY, wrong_boot);
    result(OP_ACK_SUMMARY, RESULT_BAD_ARGS);
    auto original_boot = number(7, 4);
    original_boot.push_back(0);
    command(OP_ACK_SUMMARY, original_boot);
    result(OP_ACK_SUMMARY, RESULT_OK);
}

void test_qx50m_frames_for_mobile_decoders_and_reconnect()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    claim(manager);
    s_fixtures["schema"] = characteristic(KANVEX_UUID_ENTITY_METRIC_SCHEMA).read();
    s_fixtures["colorRules"] = characteristic(KANVEX_UUID_ENTITY_METRIC_COLOR_RULES).read();
    const auto owner = manager.pairing_status();
    s_fixtures["ownerId"] = std::vector<uint8_t>(owner.owner_id, owner.owner_id + 16);
    s_fixtures["advertisement"] = NimBLEDevice::advertising.response.manufacturer;
    const auto time = number(1800000000000ULL, 8);
    command(OP_TIME_SYNC, time);
    result(OP_TIME_SYNC, RESULT_OK);
    for (uint32_t i = 1; i <= 3; ++i) sample(manager, i * 1000);
    s_fixtures["deviceInfo"] = characteristic(KANVEX_UUID_ENTITY_DEVICE_INFO).read();
    subscribe(0);
    s_fixtures["historyState"] = characteristic(KANVEX_UUID_ENTITY_HISTORY_STATE).read();
    manager.handle();
    s_fixtures["history"] = characteristic(KANVEX_UUID_ENTITY_HISTORY).value;
    ack(2);
    result(OP_ACK_HISTORY, RESULT_OK);
    command(OP_LIVE_START, {40});
    result(OP_LIVE_START, RESULT_OK);
    manager.handle();
    s_fixtures["live"] = characteristic(KANVEX_UUID_ENTITY_LIVE).value;
    CanMetrics hot = {};
    hot.engine_coolant = 120;
    hot.engine_coolant_ts = 3000;
    alert_manager.update(hot);
    mock_set_millis(4000);
    alert_manager.update(hot);
    sample(manager, 4000, 0);
    command(OP_FETCH_ALERTS, number(0, 4));
    result(OP_FETCH_ALERTS, RESULT_OK);
    manager.handle();
    s_fixtures["alert"] = characteristic(KANVEX_UUID_ENTITY_ALERTS).value;
    TEST_ASSERT_EQUAL_UINT(20, s_fixtures["alert"].size());
    command(OP_FETCH_SUMMARIES, {1});
    manager.handle();
    s_fixtures["summary"] = characteristic(KANVEX_UUID_ENTITY_SUMMARY).value;
    TEST_ASSERT_EQUAL_UINT(72, s_fixtures["summary"].size());
    TEST_ASSERT_EQUAL_UINT8(1 << (2 - 1), s_fixtures["summary"][6]);
    auto summary_ack = number(7, 4);
    summary_ack.push_back(0);
    command(OP_ACK_SUMMARY, summary_ack);
    result(OP_ACK_SUMMARY, RESULT_OK);
    manager.on_disconnect();
    manager.on_connect();
    s_fixtures["challenge"] = characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read();
    s_fixtures["auth"] = proof(s_fixtures["challenge"]);
    command(OP_AUTH, s_fixtures["auth"]);
    result(OP_AUTH, RESULT_OK);
    s_fixtures["authStatus"] = characteristic(KANVEX_UUID_ENTITY_STATUS).value;
    // NVS переживает создание нового менеджера и смену boot_counter
    manager.on_disconnect();
    BleManager rebooted;
    TEST_ASSERT_TRUE(rebooted.begin());
    rebooted.on_connect();
    const auto reboot_info = characteristic(KANVEX_UUID_ENTITY_DEVICE_INFO).read();
    TEST_ASSERT_EQUAL_UINT32(8, u32(reboot_info, 4));
    TEST_ASSERT_TRUE((reboot_info[13] & BLE_INFO_FLAG_PAIRED) != 0);
    command(OP_AUTH, proof(characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read()));
    result(OP_AUTH, RESULT_OK);
    command(OP_FETCH_SUMMARIES, {1});
    mock_notifications.clear();
    rebooted.handle();
    TEST_ASSERT_TRUE(mock_notifications.empty());
    if (const char *path = std::getenv("KANVEX_BLE_FIXTURES")) {
        std::ofstream file(path);
        TEST_ASSERT_TRUE(file.good());
        file << "{\n";
        bool first = true;
        for (const auto &item : s_fixtures) {
            if (!first) file << ",\n";
            first = false;
            file << "  \"" << item.first << "\": \"";
            for (uint8_t byte : item.second) {
                file << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(byte);
            }
            file << '"';
        }
        file << "\n}\n";
    }
}

void test_forget_phone_revokes_old_token_and_keeps_telemetry()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    claim(manager);
    sample(manager, 1000);
    sample(manager, 2000, 0);
    const auto summaries = mock_nvs["summaries"];
    const auto owner = manager.pairing_status();
    TEST_ASSERT_TRUE(owner.paired && owner.authenticated);
    TEST_ASSERT_EQUAL_UINT(16, strlen(owner.owner_id));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BleForgetResult::STALE_OWNER),
        static_cast<int>(manager.forget_phone("wrong")));
    TEST_ASSERT_TRUE(manager.pairing_status().paired);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BleForgetResult::OK),
        static_cast<int>(manager.forget_phone(owner.owner_id)));
    TEST_ASSERT_FALSE(manager.pairing_status().paired);
    TEST_ASSERT_FALSE(manager.pairing_status().authenticated);
    TEST_ASSERT_EQUAL_STRING("", manager.pairing_status().owner_id);
    TEST_ASSERT_TRUE(mock_nvs["token"].empty());
    TEST_ASSERT_TRUE(summaries == mock_nvs["summaries"]);
    manager.on_connect();
    command(OP_AUTH, proof(characteristic(KANVEX_UUID_ENTITY_AUTH_CHALLENGE).read()));
    result(OP_AUTH, RESULT_UNKNOWN_TOKEN);
    command(OP_TIME_SYNC, number(1800000000000ULL, 8));
    result(OP_TIME_SYNC, RESULT_AUTH_REQUIRED);
    auto replacement = token();
    replacement[0] ^= 0xFF;
    command(OP_PAIR_CLAIM, replacement);
    result(OP_PAIR_CLAIM, RESULT_OK);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(owner.owner_id, manager.pairing_status().owner_id));
    subscribe(0);
    manager.handle();
    TEST_ASSERT_EQUAL_UINT8(1, characteristic(KANVEX_UUID_ENTITY_HISTORY).value[2]);
    TEST_ASSERT_EQUAL_UINT32(0, u32(characteristic(KANVEX_UUID_ENTITY_HISTORY).value, 4));
}

void test_forget_phone_reports_storage_failure_without_revoking_owner()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    claim(manager);
    const auto status = manager.pairing_status();
    mock_nvs_write_failed = true;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BleForgetResult::STORAGE_ERROR),
        static_cast<int>(manager.forget_phone(status.owner_id)));
    TEST_ASSERT_TRUE(manager.pairing_status().paired);
    TEST_ASSERT_TRUE(manager.pairing_status().authenticated);
    TEST_ASSERT_EQUAL_STRING(status.owner_id, manager.pairing_status().owner_id);
    TEST_ASSERT_TRUE(mock_nvs["token"] == token());
}

void test_forgotten_phone_does_not_return_after_controller_reboot()
{
    reset();
    BleManager manager;
    TEST_ASSERT_TRUE(manager.begin());
    claim(manager);
    const auto status = manager.pairing_status();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BleForgetResult::OK),
        static_cast<int>(manager.forget_phone(status.owner_id)));
    BleManager rebooted;
    TEST_ASSERT_TRUE(rebooted.begin());
    TEST_ASSERT_FALSE(rebooted.pairing_status().paired);
    TEST_ASSERT_EQUAL_STRING(status.device_name, rebooted.pairing_status().device_name);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BleForgetResult::NOT_PAIRED),
        static_cast<int>(rebooted.forget_phone(status.owner_id)));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_qx50m_discovery_and_auth_contract);
    RUN_TEST(test_pair_claim_does_not_succeed_if_nvs_write_fails);
    RUN_TEST(test_history_mtu_and_exact_retry_before_ack);
    RUN_TEST(test_history_overwrite_while_waiting_for_ack_recovers_with_gap);
    RUN_TEST(test_backlog_drains_after_reconnect_from_committed_cursor);
    RUN_TEST(test_unacknowledged_summary_survives_controller_reboot);
    RUN_TEST(test_qx50m_frames_for_mobile_decoders_and_reconnect);
    RUN_TEST(test_forget_phone_revokes_old_token_and_keeps_telemetry);
    RUN_TEST(test_forget_phone_reports_storage_failure_without_revoking_owner);
    RUN_TEST(test_forgotten_phone_does_not_return_after_controller_reboot);
    return UNITY_END();
}
