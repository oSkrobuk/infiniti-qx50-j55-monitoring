#include <unity.h>

#include "BuzzerStub.h"
#include "DiagnosticMode.h"

void setUp()
{
    diagnostic_mode_reset();
}

void tearDown() {}

static void test_mode_stays_off_until_page_touches_it()
{
    TEST_ASSERT_FALSE(diagnostic_mode_active(1000));
}

static void test_touch_keeps_mode_active_for_timeout()
{
    diagnostic_mode_touch(1000);
    TEST_ASSERT_TRUE(diagnostic_mode_active(4000));
    TEST_ASSERT_FALSE(diagnostic_mode_active(4001));
}

static void test_new_page_request_extends_timeout()
{
    diagnostic_mode_touch(1000);
    diagnostic_mode_touch(3500);
    TEST_ASSERT_TRUE(diagnostic_mode_active(6000));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_mode_stays_off_until_page_touches_it);
    RUN_TEST(test_touch_keeps_mode_active_for_timeout);
    RUN_TEST(test_new_page_request_extends_timeout);
    return UNITY_END();
}
