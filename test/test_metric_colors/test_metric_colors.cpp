#include <unity.h>

#include <Arduino.h>
#include <LittleFS.h>

#include "BuzzerStub.h"
#include "ConfigManager.h"
#include "MetricColors.h"

// Тесты цветовых зон дисплея.
//
// Это единственное место, где водитель видит оценку состояния машины боковым
// зрением, поэтому важны границы: перепутанный знак сравнения красит перегрев
// зеленым. Пороги функции берут из глобального config, так что тесты заодно
// проверяют, что настройки из Web UI применяются без перезагрузки

static constexpr uint16_t COLOR_RED    = 0xF800;
static constexpr uint16_t COLOR_GREEN  = 0x07E0;
static constexpr uint16_t COLOR_BLUE   = 0x001F;
static constexpr uint16_t COLOR_WHITE  = 0xFFFF;
static constexpr uint16_t COLOR_YELLOW = 0xFFE0;
static constexpr uint16_t COLOR_CYAN   = 0x07FF;

void setUp(void)
{
    mock_fs_reset();
    config.reset_to_defaults();
}

void tearDown(void) {}

// ── Температура ──────────────────────────────────────────────────────────────

static void test_temperature_at_or_above_max_is_red(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_temperature_color(98.0f, 50.0f, 90.0f, 98.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_temperature_color(120.0f, 50.0f, 90.0f, 98.0f));
}

static void test_temperature_at_or_below_min_is_blue(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_temperature_color(50.0f, 50.0f, 90.0f, 98.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_temperature_color(-10.0f, 50.0f, 90.0f, 98.0f));
}

static void test_temperature_at_target_is_green(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_temperature_color(90.0f, 50.0f, 90.0f, 98.0f));
}

static void test_temperature_below_target_goes_through_cyan(void)
{
    // Середина участка min..target — голубой
    TEST_ASSERT_EQUAL_HEX16(COLOR_CYAN, metric_temperature_color(70.0f, 50.0f, 90.0f, 98.0f));
}

static void test_temperature_above_target_goes_through_yellow(void)
{
    // Середина участка target..max — желтый
    TEST_ASSERT_EQUAL_HEX16(COLOR_YELLOW, metric_temperature_color(94.0f, 50.0f, 90.0f, 98.0f));
}

static void test_temperature_with_broken_thresholds_is_white(void)
{
    // target не внутри min..max — считать градиент не из чего, показываем белый
    TEST_ASSERT_EQUAL_HEX16(COLOR_WHITE, metric_temperature_color(70.0f, 50.0f, 50.0f, 98.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_WHITE, metric_temperature_color(70.0f, 50.0f, 98.0f, 98.0f));
}

// ── Обороты ──────────────────────────────────────────────────────────────────

static void test_rpm_idle_is_blue(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_rpm_color(0.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_rpm_color(750.0f));
}

static void test_rpm_warmup_goes_through_cyan(void)
{
    // Середина участка 750..green_start (1000)
    TEST_ASSERT_EQUAL_HEX16(COLOR_CYAN, metric_rpm_color(875.0f));
}

static void test_rpm_working_range_is_green(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_rpm_color(1000.0f)); // green_start
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_rpm_color(2000.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_rpm_color(3500.0f)); // green_end
}

static void test_rpm_between_green_and_red_is_yellow(void)
{
    // Середина участка green_end (3500) .. red_start (4500)
    TEST_ASSERT_EQUAL_HEX16(COLOR_YELLOW, metric_rpm_color(4000.0f));
}

static void test_rpm_at_or_above_red_start_is_red(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_rpm_color(4500.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_rpm_color(7000.0f));
}

static void test_rpm_thresholds_come_from_config(void)
{
    // 4000 об/мин при заводских порогах — желтый (проверено выше)
    config.from_json("{\"rpm\":{\"red_start\":3800.0}}");

    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_rpm_color(4000.0f));
}

// ── Наддув ───────────────────────────────────────────────────────────────────

static void test_boost_low_is_blue(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_boost_color(1.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_boost_color(1.3f)); // blue_max
}

static void test_boost_high_is_green(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_boost_color(1.58f)); // green_min
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_boost_color(2.5f));
}

static void test_boost_between_is_transitional(void)
{
    const uint16_t color = metric_boost_color(1.44f);

    TEST_ASSERT_NOT_EQUAL(COLOR_BLUE, color);
    TEST_ASSERT_NOT_EQUAL(COLOR_GREEN, color);
    TEST_ASSERT_NOT_EQUAL(COLOR_RED, color);
}

// ── Период опроса RPM ────────────────────────────────────────────────────────

static void test_poll_time_without_data_is_blue(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_poll_time_color(0.3f, 0.0f)); // двигатель заглушен
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_poll_time_color(0.0f, 800.0f)); // замера еще не было
}

static void test_poll_time_fast_is_green(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_poll_time_color(0.1f, 800.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_poll_time_color(0.2f, 800.0f)); // green_max
}

static void test_poll_time_slow_is_red(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_poll_time_color(0.5f, 800.0f)); // red_min
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_poll_time_color(1.2f, 800.0f));
}

static void test_poll_time_between_is_transitional(void)
{
    const uint16_t color = metric_poll_time_color(0.35f, 800.0f);

    TEST_ASSERT_NOT_EQUAL(COLOR_GREEN, color);
    TEST_ASSERT_NOT_EQUAL(COLOR_RED, color);
    TEST_ASSERT_NOT_EQUAL(COLOR_BLUE, color);
}

// ── Бортовая сеть ────────────────────────────────────────────────────────────

static void test_battery_without_data_is_blue(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_battery_color(0.0f));
}

static void test_battery_out_of_range_is_red(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_battery_color(11.0f)); // ниже red_low
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_battery_color(15.2f)); // выше red_high
}

static void test_battery_normal_is_green(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_battery_color(12.0f)); // green_min
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_battery_color(13.5f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_battery_color(14.6f)); // green_max
}

static void test_battery_below_green_is_yellow(void)
{
    // Середина участка red_low (11.5) .. green_min (12.0)
    TEST_ASSERT_EQUAL_HEX16(COLOR_YELLOW, metric_battery_color(11.75f));
}

static void test_battery_above_green_is_transitional(void)
{
    const uint16_t color = metric_battery_color(14.75f);

    TEST_ASSERT_NOT_EQUAL(COLOR_GREEN, color);
    TEST_ASSERT_NOT_EQUAL(COLOR_RED, color);
    TEST_ASSERT_NOT_EQUAL(COLOR_BLUE, color);
}

// ── Давление масла ───────────────────────────────────────────────────────────

static void test_oil_pressure_without_data_is_blue(void)
{
    TEST_ASSERT_EQUAL_HEX16(COLOR_BLUE, metric_oil_pressure_color(0.0f, 2000.0f));
}

static void test_oil_pressure_uses_low_threshold_below_rpm_limit(void)
{
    // Ниже 3000 об/мин минимум 1.45 В
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED,   metric_oil_pressure_color(1.0f, 2000.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_oil_pressure_color(2.0f, 2000.0f));
}

static void test_oil_pressure_uses_high_threshold_above_rpm_limit(void)
{
    // От 3000 об/мин минимум уже 3.1 В — те же 2.0 В становятся красными
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED,   metric_oil_pressure_color(2.0f, 4000.0f));
    TEST_ASSERT_EQUAL_HEX16(COLOR_GREEN, metric_oil_pressure_color(3.5f, 4000.0f));
}

static void test_oil_pressure_at_rpm_threshold_uses_high_limit(void)
{
    // Сравнение строгое (rpm < threshold), ровно на 3000 действует верхний порог
    TEST_ASSERT_EQUAL_HEX16(COLOR_RED, metric_oil_pressure_color(2.0f, 3000.0f));
}

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_temperature_at_or_above_max_is_red);
    RUN_TEST(test_temperature_at_or_below_min_is_blue);
    RUN_TEST(test_temperature_at_target_is_green);
    RUN_TEST(test_temperature_below_target_goes_through_cyan);
    RUN_TEST(test_temperature_above_target_goes_through_yellow);
    RUN_TEST(test_temperature_with_broken_thresholds_is_white);

    RUN_TEST(test_rpm_idle_is_blue);
    RUN_TEST(test_rpm_warmup_goes_through_cyan);
    RUN_TEST(test_rpm_working_range_is_green);
    RUN_TEST(test_rpm_between_green_and_red_is_yellow);
    RUN_TEST(test_rpm_at_or_above_red_start_is_red);
    RUN_TEST(test_rpm_thresholds_come_from_config);

    RUN_TEST(test_boost_low_is_blue);
    RUN_TEST(test_boost_high_is_green);
    RUN_TEST(test_boost_between_is_transitional);

    RUN_TEST(test_poll_time_without_data_is_blue);
    RUN_TEST(test_poll_time_fast_is_green);
    RUN_TEST(test_poll_time_slow_is_red);
    RUN_TEST(test_poll_time_between_is_transitional);

    RUN_TEST(test_battery_without_data_is_blue);
    RUN_TEST(test_battery_out_of_range_is_red);
    RUN_TEST(test_battery_normal_is_green);
    RUN_TEST(test_battery_below_green_is_yellow);
    RUN_TEST(test_battery_above_green_is_transitional);

    RUN_TEST(test_oil_pressure_without_data_is_blue);
    RUN_TEST(test_oil_pressure_uses_low_threshold_below_rpm_limit);
    RUN_TEST(test_oil_pressure_uses_high_threshold_above_rpm_limit);
    RUN_TEST(test_oil_pressure_at_rpm_threshold_uses_high_limit);

    return UNITY_END();
}
