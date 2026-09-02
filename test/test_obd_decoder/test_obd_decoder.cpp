#include <unity.h>

#include <Arduino.h>

#include "BuzzerStub.h"
#include "CanTypes.h"

static constexpr uint32_t NOW = 42000;

struct DecodeCase {
    uint8_t pid;
    uint8_t a;
    uint8_t b;
    float expected;
};

static CanFrame frame(uint8_t pid, uint8_t a, uint8_t b = 0, uint8_t dlc = 8)
{
    CanFrame result = {};
    result.id = 0x7E8;
    result.dlc = dlc;
    result.data[0] = 0x04;
    result.data[1] = 0x41;
    result.data[2] = pid;
    result.data[3] = a;
    result.data[4] = b;
    return result;
}

void setUp()
{
    can_metrics = CanMetrics{};
    mock_set_millis(NOW);
}

void tearDown() {}

static void test_showcase_pid_formulas()
{
    const DecodeCase cases[] = {
        {0x04, 128, 0, 50.196f}, {0x06, 128, 0, 0.0f},
        {0x07, 64, 0, -50.0f}, {0x0A, 40, 0, 120.0f},
        {0x0B, 101, 0, 101.0f}, {0x0D, 90, 0, 90.0f},
        {0x0E, 160, 0, 16.0f}, {0x0F, 80, 0, 40.0f},
        {0x10, 0x04, 0xD2, 12.34f}, {0x11, 255, 0, 100.0f},
        {0x1F, 0x0E, 0x10, 3600.0f}, {0x23, 0x01, 0xF4, 5000.0f},
        {0x2F, 128, 0, 50.196f}, {0x33, 100, 0, 100.0f},
        {0x3C, 0x02, 0x58, 20.0f}, {0x43, 0x01, 0xFE, 200.0f},
        {0x44, 0x80, 0, 1.0f}, {0x46, 70, 0, 30.0f},
        {0x49, 128, 0, 50.196f}, {0x4A, 64, 0, 25.098f},
        {0x5E, 0x00, 0xC8, 10.0f}, {0x61, 130, 0, 5.0f},
        {0x62, 120, 0, -5.0f}, {0x63, 0x01, 0x90, 400.0f},
        {0x64, 140, 0, 15.0f},
    };

    for (const DecodeCase &item : cases) {
        can_parse_known_frames(frame(item.pid, item.a, item.b));
        TEST_ASSERT_FLOAT_WITHIN(0.01f, item.expected, can_metrics.obd[item.pid].value);
        TEST_ASSERT_EQUAL_UINT32(NOW, can_metrics.obd[item.pid].ts);
    }
}

static void test_short_two_byte_pid_is_ignored()
{
    can_parse_known_frames(frame(0x10, 0x04, 0xD2, 4));
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.obd[0x10].ts);
}

static void test_shared_pid_is_not_duplicated_in_showcase_slots()
{
    can_parse_known_frames(frame(0x05, 100));
    TEST_ASSERT_EQUAL_UINT32(NOW, can_metrics.engine_coolant_ts);
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.obd[0x05].ts);
}

static void test_unknown_supported_pid_keeps_raw_value()
{
    can_parse_known_frames(frame(0xA6, 0x12, 0x34));
    TEST_ASSERT_EQUAL_FLOAT(4660.0f, can_metrics.obd[0xA6].value);
    TEST_ASSERT_EQUAL_UINT32(NOW, can_metrics.obd[0xA6].ts);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_showcase_pid_formulas);
    RUN_TEST(test_short_two_byte_pid_is_ignored);
    RUN_TEST(test_shared_pid_is_not_duplicated_in_showcase_slots);
    RUN_TEST(test_unknown_supported_pid_keeps_raw_value);
    return UNITY_END();
}
