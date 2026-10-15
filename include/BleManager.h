#pragma once

#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "BleProtocol.h"
#include "TelemetryRing.h"

class NimBLECharacteristic;
class NimBLEServer;

struct BlePairingStatus {
    char device_name[17] = {};
    char owner_id[17] = {};
    bool paired = false;
    bool connected = false;
    bool authenticated = false;
    uint16_t history_capacity = 0;
    uint16_t history_stored = 0;
    uint16_t history_unread = 0;
    uint32_t history_oldest_seq = 0;
    uint32_t history_next_seq = 0;
    uint32_t history_acked_upto = 0;
    uint32_t history_lost_unacked = 0;
    uint8_t summaries_stored = 0;
    uint8_t summaries_unread = 0;
};

enum class BleForgetResult { OK, NOT_PAIRED, STALE_OWNER, STORAGE_ERROR };

class BleManager {
public:
    bool begin();
    void update(const CanMetrics &metrics, uint32_t now_ms, uint32_t stale_ms);
    void handle();
    void on_connect();
    void on_disconnect();
    void on_device_info_read();
    void on_challenge_read();
    void on_control_write(const uint8_t *data, size_t length);

    const char *device_name() const { return device_name_; }
    bool connected() const { return connected_; }
    BlePairingStatus pairing_status();
    BleForgetResult forget_phone(const char *expected_owner_id);

private:
    void send_status_(uint8_t opcode, uint8_t result, uint32_t extra = 0);
    void send_event_(uint8_t event, uint32_t detail);
    void send_live_();
    void send_history_();
    uint8_t history_record_capacity_();
    bool history_batch_ready_(uint32_t now_ms);
    void send_alert_();
    void send_summary_();
    void fill_device_info_();
    void refresh_advertising_(bool force = false);
    void update_trip_(uint32_t now_ms, const int16_t values[METRIC_COUNT], uint8_t presence);
    void start_trip_(uint32_t now_ms);
    void finish_trip_(uint32_t now_ms);
    void reset_trip_stats_();
    void accumulate_trip_stats_(const int16_t values[METRIC_COUNT], uint8_t presence);
    uint8_t trip_seq_for_record_(uint32_t seq) const;
    void import_alerts_();
    bool load_summaries_();
    bool save_summaries_();
    bool summaries_pending_() const;
    bool verify_auth_(const uint8_t *tag);
    bool command_requires_auth_(uint8_t opcode) const;
    bool owner_id_(char out[17]) const;
    void lock_();
    void unlock_();

    SemaphoreHandle_t mutex_ = nullptr;
    NimBLEServer *server_ = nullptr;
    NimBLECharacteristic *device_info_characteristic_ = nullptr;
    NimBLECharacteristic *status_characteristic_ = nullptr;
    NimBLECharacteristic *history_characteristic_ = nullptr;
    NimBLECharacteristic *live_characteristic_ = nullptr;
    NimBLECharacteristic *alerts_characteristic_ = nullptr;
    NimBLECharacteristic *summary_characteristic_ = nullptr;
    NimBLECharacteristic *challenge_characteristic_ = nullptr;
    TelemetryRing history_;
    char device_name_[17] = {};
    uint32_t boot_counter_ = 0;
    uint32_t last_sample_ms_ = 0;
    uint32_t live_next_ms_ = 0;
    uint32_t history_next_seq_ = 0;
    uint32_t history_pending_seq_ = 0;
    uint32_t history_last_send_ms_ = 0;
    uint32_t advertising_refresh_ms_ = 0;
    uint32_t alert_cursor_ = 0;
    uint32_t observed_alert_index_ = 0;
    uint32_t trip_start_ms_ = 0;
    uint64_t time_unix_ref_ = 0;
    uint32_t time_uptime_ref_ = 0;
    int16_t current_values_[METRIC_COUNT] = {};
    int32_t trip_min_[METRIC_COUNT] = {};
    int32_t trip_max_[METRIC_COUNT] = {};
    int64_t trip_sum_[METRIC_COUNT] = {};
    uint32_t trip_count_[METRIC_COUNT] = {};
    uint8_t current_presence_ = 0;
    uint8_t token_[PROTOCOL_TOKEN_SIZE] = {};
    uint8_t nonce_[PROTOCOL_AUTH_NONCE_SIZE] = {};
    bool connected_ = false;
    bool paired_ = false;
    bool authenticated_ = false;
    bool challenge_valid_ = false;
    bool time_synced_ = false;
    bool live_active_ = false;
    bool history_active_ = false;
    bool history_follow_ = false;
    bool history_awaiting_ack_ = false;
    bool history_pending_caught_up_ = false;
    bool history_gap_before_ = false;
    bool history_catching_up_ = false;
    bool engine_running_ = false;
    bool first_trip_ = true;
    uint8_t trip_seq_ = 0;
    uint16_t trip_alert_mask_ = 0;
    bool alert_stream_active_ = false;
    uint8_t last_advertising_flags_ = 0xFF;
    uint8_t last_advertising_trip_seq_ = 0xFF;

    struct StoredAlert {
        BleAlertFrame frame;
    };
    static constexpr uint8_t ALERT_RING_SIZE = 32;
    StoredAlert alert_ring_[ALERT_RING_SIZE] = {};
    uint32_t alert_next_index_ = 0;

    struct StoredSummary {
        BleSummaryFrame frame;
        uint8_t uploaded;
    };
    static constexpr uint8_t SUMMARY_CAPACITY = 50;
    StoredSummary summaries_[SUMMARY_CAPACITY] = {};
    uint8_t summary_count_ = 0;
    uint8_t summary_cursor_ = 0;
    int8_t summary_pending_index_ = -1;
    bool summary_stream_active_ = false;
    bool summary_pending_only_ = false;
};

extern BleManager ble_manager;

