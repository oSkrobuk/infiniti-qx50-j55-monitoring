#include <unity.h>

#include "BuzzerStub.h"
#include "DiagnosticSelection.h"

void setUp()
{
    diagnostic_selection.reset();
}

void tearDown() {}

static void test_all_pids_are_disabled_by_default()
{
    for (uint16_t pid = 0; pid <= 0xFF; ++pid) {
        TEST_ASSERT_FALSE(diagnostic_selection.pid_enabled(static_cast<uint8_t>(pid)));
    }
}

static void test_pid_can_be_enabled_and_disabled()
{
    diagnostic_selection.set_pid(0x0C, true);
    TEST_ASSERT_TRUE(diagnostic_selection.pid_enabled(0x0C));
    diagnostic_selection.set_pid(0x0C, false);
    TEST_ASSERT_FALSE(diagnostic_selection.pid_enabled(0x0C));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_pids_are_disabled_by_default);
    RUN_TEST(test_pid_can_be_enabled_and_disabled);
    return UNITY_END();
}
