#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CanTypes.h"
#include "protocol_generated.h"

static constexpr uint8_t BLE_STATUS_KIND_REPLY = 0x00;
static constexpr uint8_t BLE_STATUS_KIND_EVENT = 0x01;
static constexpr uint8_t BLE_HISTORY_LEVEL_TRIP = 0;
static constexpr uint8_t BLE_SUBSCRIBE_FLAG_FOLLOW = 1 << 0;
static constexpr uint32_t BLE_FROM_SEQ_ONLY_NEW = 0xFFFFFFFFu;
static constexpr uint8_t BLE_INFO_FLAG_ENGINE_RUNNING = 1 << 0;
static constexpr uint8_t BLE_INFO_FLAG_TIME_SYNCED = 1 << 1;
static constexpr uint8_t BLE_INFO_FLAG_SUMMARIES_PENDING = 1 << 2;
static constexpr uint8_t BLE_INFO_FLAG_PAIRED = 1 << 3;
static constexpr uint8_t BLE_INFO_FLAG_PAIRING_ACTIVE = 1 << 4;
static constexpr uint8_t BLE_LIVE_FLAG_ENGINE_RUNNING = 1 << 0;
static constexpr uint8_t BLE_HISTORY_FLAG_CAUGHT_UP = 1 << 0;
static constexpr uint8_t BLE_HISTORY_FLAG_GAP_BEFORE = 1 << 1;

#pragma pack(push, 1)
struct BleDeviceInfoFrame {
    uint8_t protocol_version;
    uint8_t frame_format;
    uint16_t firmware_version;
    uint32_t boot_counter;
    uint32_t uptime_ms;
    uint8_t trip_seq;
    uint8_t flags;
    uint16_t metric_mask;
    uint32_t trip_oldest_seq;
    uint32_t trip_newest_seq;
    uint32_t full_oldest_seq;
    uint32_t full_newest_seq;
    uint16_t trip_ring_records;
    uint16_t full_ring_records;
    uint8_t min_protocol_version;
    uint8_t max_protocol_version;
    uint32_t capabilities;
};

struct BleStatusFrame {
    uint8_t kind;
    uint8_t code;
    uint16_t reserved;
    uint32_t detail;
};

struct BleHistoryHeader {
    uint8_t level;
    uint8_t frame_format;
    uint8_t count;
    uint8_t flags;
    uint32_t first_seq;
    uint32_t first_uptime_ms;
    uint32_t boot_counter;
    uint8_t trip_seq;
    uint8_t schema_id[3];
};

struct BleLiveHeader {
    uint32_t uptime_ms;
    uint32_t boot_counter;
    uint8_t trip_seq;
    uint8_t flags;
    uint8_t schema_id[3];
};

struct BleAuthChallengeFrame {
    uint8_t nonce[PROTOCOL_AUTH_NONCE_SIZE];
    uint32_t boot_counter;
};

struct BleAlertFrame {
    uint32_t index;
    uint32_t uptime_ms;
    uint32_t boot_counter;
    uint8_t trip_seq;
    uint8_t code;
    uint8_t state;
    uint8_t metric_id;
    int16_t value;
    uint16_t reserved;
};

struct BleSummaryFrame {
    uint32_t boot_counter;
    uint8_t trip_seq;
    uint8_t flags;
    uint16_t alert_mask;
    uint64_t start_unix_ms;
    uint32_t duration_s;
    int16_t stats[METRIC_COUNT][3];
    uint32_t reserved;
};

struct BleAdvStatusData {
    uint16_t company_id;
    uint8_t status_version;
    uint8_t flags;
    uint32_t boot_counter;
    uint8_t trip_seq;
};
#pragma pack(pop)

static_assert(sizeof(BleDeviceInfoFrame) == PROTOCOL_SIZE_DEVICE_INFO, "размер device_info");
static_assert(sizeof(BleStatusFrame) == PROTOCOL_SIZE_STATUS, "размер status");
static_assert(sizeof(BleHistoryHeader) == PROTOCOL_SIZE_HISTORY_HEADER, "размер history");
static_assert(sizeof(BleLiveHeader) == PROTOCOL_SIZE_LIVE_HEADER, "размер live");
static_assert(sizeof(BleAuthChallengeFrame) == PROTOCOL_SIZE_AUTH_CHALLENGE, "размер challenge");
static_assert(sizeof(BleAlertFrame) == PROTOCOL_SIZE_ALERT, "размер alert");
static_assert(sizeof(BleSummaryFrame) == 72, "размер summary");
static_assert(sizeof(BleAdvStatusData) == PROTOCOL_SIZE_SCAN_STATUS, "размер advertising status");

uint32_t ble_metric_schema_id();
uint8_t ble_metric_values_size();
uint16_t ble_encode_metric_schema(uint8_t *out, size_t capacity);
uint8_t ble_encode_metric_values(const int16_t values[METRIC_COUNT], uint8_t presence,
                                 uint8_t *out, size_t capacity);
uint8_t ble_metrics_from_can(const CanMetrics &metrics, uint32_t now_ms, uint32_t stale_ms,
                             int16_t values[METRIC_COUNT]);

