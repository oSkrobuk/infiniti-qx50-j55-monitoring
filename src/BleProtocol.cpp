#include "BleProtocol.h"

#include <limits.h>
#include <math.h>
#include <string.h>

namespace {

static bool is_fresh(uint32_t timestamp, uint32_t now_ms, uint32_t stale_ms)
{
    return timestamp != 0 && now_ms - timestamp <= stale_ms;
}

static int16_t scaled_value(float value, float scale)
{
    const float scaled = roundf(value * scale);
    if (scaled < static_cast<float>(INT16_MIN)) return INT16_MIN;
    if (scaled > static_cast<float>(INT16_MAX - 1)) return INT16_MAX - 1;
    return static_cast<int16_t>(scaled);
}

static void write_u24(uint8_t out[3], uint32_t value)
{
    out[0] = static_cast<uint8_t>(value);
    out[1] = static_cast<uint8_t>(value >> 8);
    out[2] = static_cast<uint8_t>(value >> 16);
}

} // namespace

uint32_t ble_metric_schema_id()
{
    uint32_t hash = 2166136261u;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(METRIC_DESCRIPTORS);
    for (size_t i = 0; i < sizeof(METRIC_DESCRIPTORS); ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    hash &= 0x00FFFFFFu;
    return hash == 0 ? 1 : hash;
}

uint8_t ble_metric_values_size()
{
    return static_cast<uint8_t>((METRIC_COUNT + 7u) / 8u + METRIC_COUNT * sizeof(int16_t));
}

uint16_t ble_encode_metric_schema(uint8_t *out, size_t capacity)
{
    const uint16_t size = PROTOCOL_SIZE_METRIC_SCHEMA_HEADER + sizeof(METRIC_DESCRIPTORS);
    if (out == nullptr || capacity < size) return 0;

    memset(out, 0, size);
    out[0] = METRIC_SCHEMA_FORMAT;
    out[1] = METRIC_COUNT;
    write_u24(out + 2, ble_metric_schema_id());
    out[5] = ble_metric_values_size();
    memcpy(out + PROTOCOL_SIZE_METRIC_SCHEMA_HEADER, METRIC_DESCRIPTORS,
           sizeof(METRIC_DESCRIPTORS));
    return size;
}

uint8_t ble_encode_metric_values(const int16_t values[METRIC_COUNT], uint8_t presence,
                                 uint8_t *out, size_t capacity)
{
    const uint8_t size = ble_metric_values_size();
    if (values == nullptr || out == nullptr || capacity < size) return 0;

    memset(out, 0, size);
    out[0] = presence;
    memcpy(out + 1, values, METRIC_COUNT * sizeof(int16_t));
    return size;
}

uint8_t ble_metrics_from_can(const CanMetrics &metrics, uint32_t now_ms, uint32_t stale_ms,
                             int16_t values[METRIC_COUNT])
{
    for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
        values[i] = static_cast<int16_t>(PROTOCOL_NO_VALUE_SIGNED);
    }
    uint8_t presence = 0;

    if (is_fresh(metrics.engine_coolant_ts, now_ms, stale_ms)) {
        values[0] = scaled_value(metrics.engine_coolant, 10.0f);
        presence |= 1 << 0;
    }
    if (is_fresh(metrics.engine_oil_ts, now_ms, stale_ms)) {
        values[1] = scaled_value(metrics.engine_oil, 10.0f);
        presence |= 1 << 1;
    }
    if (is_fresh(metrics.cvt_temp_ts, now_ms, stale_ms)) {
        values[2] = scaled_value(metrics.cvt_temp, 10.0f);
        presence |= 1 << 2;
    }
    if (is_fresh(metrics.engine_rpm_ts, now_ms, stale_ms)) {
        values[3] = scaled_value(metrics.engine_rpm, 1.0f);
        presence |= 1 << 3;
    }
    if (is_fresh(metrics.turbo_boost_volt_ts, now_ms, stale_ms)) {
        values[4] = scaled_value(metrics.turbo_boost_volt, 100.0f);
        presence |= 1 << 4;
    }
    if (is_fresh(metrics.oil_pressure_volt_ts, now_ms, stale_ms)) {
        values[5] = scaled_value(metrics.oil_pressure_volt, 100.0f);
        presence |= 1 << 5;
    }
    if (is_fresh(metrics.battery_voltage_ts, now_ms, stale_ms)) {
        values[6] = scaled_value(metrics.battery_voltage, 100.0f);
        presence |= 1 << 6;
    }
    if (is_fresh(metrics.radiator_coolant_ts, now_ms, stale_ms)) {
        values[7] = scaled_value(metrics.radiator_coolant, 10.0f);
        presence |= 1 << 7;
    }
    return presence;
}

