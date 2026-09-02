#include <unity.h>

#include "BuzzerStub.h"
#include "ObdPidCatalog.h"

static CanFrame support_frame(uint8_t base, uint32_t mask)
{
    CanFrame frame = {};
    frame.id = 0x7E8;
    frame.dlc = 8;
    frame.data[0] = 0x06;
    frame.data[1] = 0x41;
    frame.data[2] = base;
    frame.data[3] = static_cast<uint8_t>(mask >> 24);
    frame.data[4] = static_cast<uint8_t>(mask >> 16);
    frame.data[5] = static_cast<uint8_t>(mask >> 8);
    frame.data[6] = static_cast<uint8_t>(mask);
    return frame;
}

void setUp() {}
void tearDown() {}

static void test_maps_mask_bits_to_pid_numbers()
{
    ObdPidCatalog catalog;
    TEST_ASSERT_TRUE(catalog.accept(support_frame(0x00, 0x08180001)));
    TEST_ASSERT_TRUE(catalog.supports(0x05));
    TEST_ASSERT_TRUE(catalog.supports(0x0C));
    TEST_ASSERT_TRUE(catalog.supports(0x0D));
    TEST_ASSERT_TRUE(catalog.supports(0x20));
    TEST_ASSERT_FALSE(catalog.supports(0x04));
    TEST_ASSERT_EQUAL_HEX8(0x20, catalog.next_query_base());
    TEST_ASSERT_FALSE(catalog.complete());
}

static void test_stops_when_next_range_bit_is_clear()
{
    ObdPidCatalog catalog;
    TEST_ASSERT_TRUE(catalog.accept(support_frame(0x00, 0x08000000)));
    TEST_ASSERT_TRUE(catalog.complete());
    TEST_ASSERT_EQUAL_HEX8(0xFF, catalog.next_query_base());
}

static void test_rejects_out_of_order_and_malformed_frames()
{
    ObdPidCatalog catalog;
    TEST_ASSERT_FALSE(catalog.accept(support_frame(0x20, 0x80000000)));

    CanFrame short_frame = support_frame(0x00, 0x80000000);
    short_frame.dlc = 6;
    TEST_ASSERT_FALSE(catalog.accept(short_frame));
    TEST_ASSERT_FALSE(catalog.supports(0x01));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_maps_mask_bits_to_pid_numbers);
    RUN_TEST(test_stops_when_next_range_bit_is_clear);
    RUN_TEST(test_rejects_out_of_order_and_malformed_frames);
    return UNITY_END();
}
