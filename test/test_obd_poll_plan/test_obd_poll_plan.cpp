#include <unity.h>

#include "BuzzerStub.h"
#include "ObdPollPlan.h"

static CanFrame support_frame(uint32_t mask)
{
    CanFrame frame = {};
    frame.id = 0x7E8;
    frame.dlc = 8;
    frame.data[0] = 0x06;
    frame.data[1] = 0x41;
    frame.data[3] = static_cast<uint8_t>(mask >> 24);
    frame.data[4] = static_cast<uint8_t>(mask >> 16);
    frame.data[5] = static_cast<uint8_t>(mask >> 8);
    frame.data[6] = static_cast<uint8_t>(mask);
    return frame;
}

void setUp() {}
void tearDown() {}

static void test_primary_poll_always_wins()
{
    ObdPidCatalog catalog;
    catalog.accept(support_frame(0x08100000));
    const uint8_t pids[] = {0x05, 0x0C};
    ObdPollPlan plan(catalog, pids, 2, 1000);

    TEST_ASSERT_EQUAL_INT16(-1, plan.next(1000, true));
    TEST_ASSERT_EQUAL_INT16(0x05, plan.next(1000, false));
}

static void test_failed_send_retries_same_pid()
{
    ObdPidCatalog catalog;
    catalog.accept(support_frame(0x08100000));
    const uint8_t pids[] = {0x05, 0x0C};
    ObdPollPlan plan(catalog, pids, 2, 1000);

    TEST_ASSERT_EQUAL_INT16(0x05, plan.next(1000, false));
    plan.complete_send(false);
    TEST_ASSERT_EQUAL_INT16(0x05, plan.next(1001, false));
    plan.complete_send(true);
    TEST_ASSERT_EQUAL_INT16(0x0C, plan.next(1001, false));
}

static void test_unsupported_pids_are_skipped_until_next_cycle()
{
    ObdPidCatalog catalog;
    catalog.accept(support_frame(0x08000000));
    const uint8_t pids[] = {0x04, 0x05, 0x0C};
    ObdPollPlan plan(catalog, pids, 3, 1000);

    TEST_ASSERT_EQUAL_INT16(0x05, plan.next(1000, false));
    plan.complete_send(true);
    TEST_ASSERT_EQUAL_INT16(-1, plan.next(1000, false));
    TEST_ASSERT_EQUAL_INT16(0x05, plan.next(2000, false));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_primary_poll_always_wins);
    RUN_TEST(test_failed_send_retries_same_pid);
    RUN_TEST(test_unsupported_pids_are_skipped_until_next_cycle);
    return UNITY_END();
}
