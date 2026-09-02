#include <unity.h>

#include "BuzzerStub.h"
#include "CanRecovery.h"

void setUp()
{
}

void tearDown()
{
}

static void test_failed_initiate_is_retried_after_backoff()
{
    CanRecovery recovery;

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::INITIATE),
        static_cast<uint8_t>(recovery.observe(CanDriverState::BUS_OFF, 1000))
    );
    recovery.complete(CanRecoveryAction::INITIATE, false, 1000);

    TEST_ASSERT_EQUAL_UINT32(1, recovery.bus_off_count());
    TEST_ASSERT_EQUAL_UINT32(1, recovery.initiate_attempt_count());
    TEST_ASSERT_EQUAL_UINT32(1, recovery.initiate_failure_count());
    TEST_ASSERT_EQUAL_UINT32(1250, recovery.next_attempt_ts());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::NONE),
        static_cast<uint8_t>(recovery.observe(CanDriverState::BUS_OFF, 1249))
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::INITIATE),
        static_cast<uint8_t>(recovery.observe(CanDriverState::BUS_OFF, 1250))
    );
}

static void test_backoff_doubles_and_is_capped()
{
    CanRecovery recovery;
    uint32_t now = 0;
    uint32_t expected_delay = CanRecovery::INITIAL_BACKOFF_MS;

    for (int attempt = 0; attempt < 10; attempt++) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(CanRecoveryAction::INITIATE),
            static_cast<uint8_t>(recovery.observe(CanDriverState::BUS_OFF, now))
        );
        recovery.complete(CanRecoveryAction::INITIATE, false, now);
        TEST_ASSERT_EQUAL_UINT32(now + expected_delay, recovery.next_attempt_ts());
        now += expected_delay;
        expected_delay = expected_delay * 2 > CanRecovery::MAX_BACKOFF_MS
            ? CanRecovery::MAX_BACKOFF_MS
            : expected_delay * 2;
    }
}

static void test_failed_start_is_retried_without_new_bus_off()
{
    CanRecovery recovery;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::INITIATE),
        static_cast<uint8_t>(recovery.observe(CanDriverState::BUS_OFF, 100))
    );
    recovery.complete(CanRecoveryAction::INITIATE, true, 100);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::START),
        static_cast<uint8_t>(recovery.observe(CanDriverState::STOPPED, 200))
    );
    recovery.complete(CanRecoveryAction::START, false, 200);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::NONE),
        static_cast<uint8_t>(recovery.observe(CanDriverState::STOPPED, 449))
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::START),
        static_cast<uint8_t>(recovery.observe(CanDriverState::STOPPED, 450))
    );
    recovery.complete(CanRecoveryAction::START, true, 450);

    TEST_ASSERT_EQUAL_UINT32(1, recovery.bus_off_count());
    TEST_ASSERT_EQUAL_UINT32(1, recovery.recovery_count());
    TEST_ASSERT_EQUAL_UINT32(2, recovery.start_attempt_count());
    TEST_ASSERT_EQUAL_UINT32(1, recovery.start_failure_count());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanDriverState::RUNNING),
        static_cast<uint8_t>(recovery.state())
    );
}

static void test_driver_bus_off_during_accepted_recovery_is_not_counted_twice()
{
    CanRecovery recovery;
    recovery.observe(CanDriverState::BUS_OFF, 0);
    recovery.complete(CanRecoveryAction::INITIATE, true, 0);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CanRecoveryAction::NONE),
        static_cast<uint8_t>(recovery.observe(CanDriverState::BUS_OFF, 10))
    );
    TEST_ASSERT_EQUAL_UINT32(1, recovery.bus_off_count());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_failed_initiate_is_retried_after_backoff);
    RUN_TEST(test_backoff_doubles_and_is_capped);
    RUN_TEST(test_failed_start_is_retried_without_new_bus_off);
    RUN_TEST(test_driver_bus_off_during_accepted_recovery_is_not_counted_twice);
    return UNITY_END();
}
