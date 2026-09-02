#include <unity.h>

#include <Arduino.h>

#include "BuzzerStub.h"
#include "CanTypes.h"

// Тесты декодера CAN: сырые байты UDS-ответа → физические величины в can_metrics.
// Здесь проверяется калибровка (смещения -50 и -40, множители 12.5, 0.08, 1/50,
// 1/200) — именно она молча ломается при правках и не видна ни на дисплее,
// ни в логах, пока машина не покажет неправдоподобную температуру

// Момент времени, от которого идут тесты. Ненулевой намеренно: поля *_ts
// сравниваются с нулем как признак «данных не было», и при millis() == 0
// свежие данные были бы неотличимы от их отсутствия
static constexpr unsigned long TEST_NOW_MS = 250000;

// Собрать кадр положительного ответа UDS (Service 0x22 → SID 0x62)
static CanFrame make_uds_frame(uint32_t id, uint16_t did,
                               uint8_t b4, uint8_t b5 = 0, uint8_t dlc = 8)
{
    CanFrame f = {};
    f.id      = id;
    f.dlc     = dlc;
    f.data[0] = 0x05; // Длина полезной нагрузки ISO-TP
    f.data[1] = 0x62; // Положительный ответ на чтение параметров
    f.data[2] = static_cast<uint8_t>(did >> 8);
    f.data[3] = static_cast<uint8_t>(did & 0xFF);
    f.data[4] = b4;
    f.data[5] = b5;
    return f;
}

// Собрать First Frame многокадрового ответа блока состояния освещения
static CanFrame make_light_frame(uint8_t status)
{
    CanFrame f = {};
    f.id      = 0x763;
    f.dlc     = 8;
    f.data[0] = 0x10;
    f.data[1] = 0x16;
    f.data[2] = 0x62;
    f.data[3] = 0x0E;
    f.data[4] = 0x07;
    f.data[5] = 0x00;
    f.data[6] = status;
    f.data[7] = 0x10;
    return f;
}

void setUp(void)
{
    can_metrics = CanMetrics{};
    mock_set_millis(TEST_NOW_MS);
}

void tearDown(void) {}

// ── Температуры: сырой байт минус смещение ───────────────────────────────────

static void test_engine_coolant_temp_decoded(void)
{
    // 100 - 50 = 50 °C
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1101, 100));
    TEST_ASSERT_EQUAL_FLOAT(50.0f, can_metrics.engine_coolant);
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS, can_metrics.engine_coolant_ts);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MetricSource::INFINITI_UDS),
                            static_cast<uint8_t>(can_metrics.engine_coolant_source));
}

static void test_engine_oil_temp_decoded(void)
{
    // 140 - 50 = 90 °C
    can_parse_known_frames(make_uds_frame(0x7E8, 0x111F, 140));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, can_metrics.engine_oil);
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS, can_metrics.engine_oil_ts);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MetricSource::INFINITI_UDS),
                            static_cast<uint8_t>(can_metrics.engine_oil_source));
}

static void test_radiator_coolant_temp_decoded(void)
{
    // 90 - 50 = 40 °C
    can_parse_known_frames(make_uds_frame(0x7E8, 0x116B, 90));
    TEST_ASSERT_EQUAL_FLOAT(40.0f, can_metrics.radiator_coolant);
}

static void test_temperature_below_offset_is_negative(void)
{
    // 20 - 50 = -30 °C: зимний холодный пуск
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1101, 20));
    TEST_ASSERT_EQUAL_FLOAT(-30.0f, can_metrics.engine_coolant);
}

static void test_temperature_max_raw_byte(void)
{
    // Сырой байт беззнаковый: 255 - 50 = 205 °C, а не -51
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1101, 255));
    TEST_ASSERT_EQUAL_FLOAT(205.0f, can_metrics.engine_coolant);
}

static void test_cvt_temp_decoded_from_tcm(void)
{
    // Вариатор отвечает с другого адреса и со смещением -40: 102 - 40 = 62 °C
    can_parse_known_frames(make_uds_frame(0x7E9, 0x110C, 102, 0, 5));
    TEST_ASSERT_EQUAL_FLOAT(62.0f, can_metrics.cvt_temp);
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS, can_metrics.cvt_temp_ts);
}

static void test_cvt_did_from_ecm_is_ignored(void)
{
    // Тот же DID, но от ECM (0x7E8) — не наш параметр, метрика не трогается
    can_parse_known_frames(make_uds_frame(0x7E8, 0x110C, 102));
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.cvt_temp_ts);
}

static void test_exterior_light_status_decoded(void)
{
    can_parse_known_frames(make_light_frame(0x0C));
    TEST_ASSERT_FALSE(can_metrics.exterior_light_on);
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS, can_metrics.exterior_light_ts);

    mock_advance_millis(5000);
    can_parse_known_frames(make_light_frame(0x1C));
    TEST_ASSERT_TRUE(can_metrics.exterior_light_on);
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS + 5000, can_metrics.exterior_light_ts);
}

// ── Обороты, наддув, давление масла, бортовая сеть ───────────────────────────

static void test_engine_rpm_decoded(void)
{
    // 0x0040 = 64, 64 * 12.5 = 800 об/мин (холостой ход)
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1201, 0x00, 0x40));
    TEST_ASSERT_EQUAL_FLOAT(800.0f, can_metrics.engine_rpm);
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS, can_metrics.engine_rpm_ts);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MetricSource::INFINITI_UDS),
                            static_cast<uint8_t>(can_metrics.engine_rpm_source));
}

static void test_engine_rpm_uses_both_bytes(void)
{
    // 0x0190 = 400, 400 * 12.5 = 5000 об/мин — старший байт обязан учитываться
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1201, 0x01, 0x90));
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, can_metrics.engine_rpm);
}

static void test_turbo_boost_decoded(void)
{
    // 79 / 50 = 1.58 В
    can_parse_known_frames(make_uds_frame(0x7E8, 0x110E, 79));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.58f, can_metrics.turbo_boost_volt);
}

static void test_oil_pressure_decoded(void)
{
    // 0x014E = 334, 334 / 200 = 1.67 В
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1278, 0x01, 0x4E));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.67f, can_metrics.oil_pressure_volt);
}

static void test_battery_voltage_decoded(void)
{
    // 178 * 0.08 = 14.24 В
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1103, 178));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.24f, can_metrics.battery_voltage);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MetricSource::INFINITI_UDS),
                            static_cast<uint8_t>(can_metrics.battery_voltage_source));
}

// ── Отбраковка мусора ────────────────────────────────────────────────────────

static void test_short_ecm_frame_is_ignored(void)
{
    // ECM-ветке нужны байты d[0]...d[5]: при dlc = 5 читать нечего
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1101, 100, 0, 5));
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_coolant_ts);
}

static void test_short_tcm_frame_is_ignored(void)
{
    // TCM-ветке хватает d[0]...d[4], но при dlc = 4 не хватает и их
    can_parse_known_frames(make_uds_frame(0x7E9, 0x110C, 102, 0, 4));
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.cvt_temp_ts);
}

static void test_empty_frame_is_ignored(void)
{
    CanFrame f = {};
    f.id  = 0x7E8;
    f.dlc = 0;
    can_parse_known_frames(f);
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_coolant_ts);
}

static void test_negative_response_is_ignored(void)
{
    // 0x7F — отрицательный ответ UDS, полезных данных в нем нет
    CanFrame f = make_uds_frame(0x7E8, 0x1101, 100);
    f.data[1]  = 0x7F;
    can_parse_known_frames(f);
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_coolant_ts);
}

static void test_unknown_did_is_ignored(void)
{
    can_parse_known_frames(make_uds_frame(0x7E8, 0xABCD, 100));
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_coolant_ts);
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_rpm_ts);
}

static void test_unknown_frame_id_is_ignored(void)
{
    // 0x7DF — широковещательный запрос диагностики, а не ответ
    can_parse_known_frames(make_uds_frame(0x7DF, 0x1101, 100));
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_coolant_ts);
}

// ── Обновление отметок времени ───────────────────────────────────────────────

static void test_timestamp_follows_millis(void)
{
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1101, 100));
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS, can_metrics.engine_coolant_ts);

    mock_advance_millis(3000);
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1101, 110));

    TEST_ASSERT_EQUAL_FLOAT(60.0f, can_metrics.engine_coolant);
    TEST_ASSERT_EQUAL_UINT32(TEST_NOW_MS + 3000, can_metrics.engine_coolant_ts);
}

static void test_one_did_does_not_touch_others(void)
{
    can_parse_known_frames(make_uds_frame(0x7E8, 0x1101, 100));

    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_oil_ts);
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.engine_rpm_ts);
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.battery_voltage_ts);
    TEST_ASSERT_EQUAL_UINT32(0, can_metrics.cvt_temp_ts);
}

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_engine_coolant_temp_decoded);
    RUN_TEST(test_engine_oil_temp_decoded);
    RUN_TEST(test_radiator_coolant_temp_decoded);
    RUN_TEST(test_temperature_below_offset_is_negative);
    RUN_TEST(test_temperature_max_raw_byte);
    RUN_TEST(test_cvt_temp_decoded_from_tcm);
    RUN_TEST(test_cvt_did_from_ecm_is_ignored);
    RUN_TEST(test_exterior_light_status_decoded);

    RUN_TEST(test_engine_rpm_decoded);
    RUN_TEST(test_engine_rpm_uses_both_bytes);
    RUN_TEST(test_turbo_boost_decoded);
    RUN_TEST(test_oil_pressure_decoded);
    RUN_TEST(test_battery_voltage_decoded);

    RUN_TEST(test_short_ecm_frame_is_ignored);
    RUN_TEST(test_short_tcm_frame_is_ignored);
    RUN_TEST(test_empty_frame_is_ignored);
    RUN_TEST(test_negative_response_is_ignored);
    RUN_TEST(test_unknown_did_is_ignored);
    RUN_TEST(test_unknown_frame_id_is_ignored);

    RUN_TEST(test_timestamp_follows_millis);
    RUN_TEST(test_one_did_does_not_touch_others);

    return UNITY_END();
}
