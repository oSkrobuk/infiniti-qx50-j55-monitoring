#include <unity.h>

#include "BleProtocol.h"
#include "BuzzerStub.h"

void test_schema_matches_generated_contract()
{
    uint8_t schema[sizeof(METRIC_SCHEMA_GOLDEN)] = {};
    const uint16_t size = ble_encode_metric_schema(schema, sizeof(schema));
    TEST_ASSERT_EQUAL_UINT16(sizeof(METRIC_SCHEMA_GOLDEN), size);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(METRIC_SCHEMA_GOLDEN, schema, size);
}

void test_can_metrics_are_scaled_and_stale_values_are_absent()
{
    CanMetrics metrics = {};
    metrics.engine_coolant = 91.2f;
    metrics.engine_coolant_ts = 900;
    metrics.engine_rpm = 1750.0f;
    metrics.engine_rpm_ts = 900;
    metrics.battery_voltage = 14.24f;
    metrics.battery_voltage_ts = 100;
    int16_t values[METRIC_COUNT] = {};

    const uint32_t presence = ble_metrics_from_can(metrics, 1000, 500, values);

    TEST_ASSERT_EQUAL_HEX8((1 << METRIC_COOLANT_TEMP) | (1 << METRIC_RPM), presence);
    TEST_ASSERT_EQUAL_INT16(912, values[METRIC_COOLANT_TEMP]);
    TEST_ASSERT_EQUAL_INT16(1750, values[METRIC_RPM]);
    TEST_ASSERT_EQUAL_INT16(PROTOCOL_NO_VALUE_SIGNED, values[METRIC_BATTERY_VOLTAGE]);
}

void test_metric_values_use_presence_bitmap()
{
    int16_t values[METRIC_COUNT] = {};
    values[METRIC_ENGINE_OIL_TEMP] = 987;
    values[METRIC_BATTERY_VOLTAGE] = 1420;
    uint8_t encoded[(METRIC_COUNT + 7u) / 8u + METRIC_COUNT * sizeof(int16_t)] = {};
    const uint32_t presence = (1u << METRIC_ENGINE_OIL_TEMP) | (1u << METRIC_BATTERY_VOLTAGE);

    TEST_ASSERT_EQUAL_UINT8(sizeof(encoded),
                            ble_encode_metric_values(values, presence, encoded, sizeof(encoded)));
    TEST_ASSERT_EQUAL_HEX8(presence, encoded[0]);
    TEST_ASSERT_EQUAL_HEX8(0xDB, encoded[3]);
    TEST_ASSERT_EQUAL_HEX8(0x03, encoded[4]);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_schema_matches_generated_contract);
    RUN_TEST(test_can_metrics_are_scaled_and_stale_values_are_absent);
    RUN_TEST(test_metric_values_use_presence_bitmap);
    return UNITY_END();
}
