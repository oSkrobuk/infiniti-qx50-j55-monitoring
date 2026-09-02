#include <unity.h>

#include "BuzzerStub.h"
#include "protocol_generated.h"

void test_protocol_version_and_capabilities()
{
    TEST_ASSERT_EQUAL_UINT8(5, PROTOCOL_VERSION);
    TEST_ASSERT_EQUAL_UINT8(PROTOCOL_VERSION, PROTOCOL_MIN_VERSION);
    TEST_ASSERT_EQUAL_HEX32(0x3F, CAPABILITY_ALL);
}

void test_metric_catalog_matches_current_vehicle_metrics()
{
    TEST_ASSERT_EQUAL_UINT8(8, METRIC_COUNT);
    TEST_ASSERT_EQUAL_UINT8(METRIC_COOLANT_TEMP, METRIC_DESCRIPTORS[0].id);
    TEST_ASSERT_EQUAL_UINT8(METRIC_RADIATOR_COOLANT_TEMP, METRIC_DESCRIPTORS[7].id);
    TEST_ASSERT_EQUAL_HEX32(0xBC5424, METRIC_SCHEMA_ID);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_protocol_version_and_capabilities);
    RUN_TEST(test_metric_catalog_matches_current_vehicle_metrics);
    return UNITY_END();
}
