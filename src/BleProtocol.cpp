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

} // namespace

uint32_t ble_metric_schema_id()
{
    return METRIC_SCHEMA_ID;
}

uint8_t ble_metric_values_size()
{
    return static_cast<uint8_t>((METRIC_COUNT + 7u) / 8u + METRIC_COUNT * sizeof(int16_t));
}

uint16_t ble_encode_metric_schema(uint8_t *out, size_t capacity)
{
    const uint16_t size = sizeof(METRIC_SCHEMA_GOLDEN);
    if (out == nullptr || capacity < size) return 0;

    memcpy(out, METRIC_SCHEMA_GOLDEN, size);
    return size;
}

uint8_t ble_encode_metric_values(const int16_t values[METRIC_COUNT], uint32_t presence,
                                 uint8_t *out, size_t capacity)
{
    const uint8_t size = ble_metric_values_size();
    if (values == nullptr || out == nullptr || capacity < size) return 0;

    memset(out, 0, size);
    const uint8_t bitmap_size = (METRIC_COUNT + 7u) / 8u;
    for (uint8_t i = 0; i < bitmap_size; ++i) out[i] = static_cast<uint8_t>(presence >> (8u * i));
    memcpy(out + bitmap_size, values, METRIC_COUNT * sizeof(int16_t));
    return size;
}

uint32_t ble_metrics_from_can(const CanMetrics &metrics, uint32_t now_ms, uint32_t stale_ms,
                             int16_t values[METRIC_COUNT])
{
    for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
        values[i] = static_cast<int16_t>(PROTOCOL_NO_VALUE_SIGNED);
    }
    uint32_t presence = 0;

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

