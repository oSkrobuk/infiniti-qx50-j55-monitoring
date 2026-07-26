#include <unity.h>

#include <Arduino.h>
#include <LittleFS.h>

#include "AlertManager.h"
#include "BuzzerStub.h"

// Тесты движка проверок: пороги, режимы повтора, журнал и персистентность.
//
// Каждый тест работает со своим экземпляром AlertManager — конструктор полностью
// сбрасывает состояние, поэтому глобальный alert_manager из прошивки тесты
// не задевают и порядок их выполнения не важен

// Стартовое время. Ненулевое намеренно: антидребезг сравнивает last_trigger_ms_
// с нулем как с признаком «еще ни разу не срабатывало», и при millis() == 0
// первое срабатывание было бы неотличимо от отсутствия срабатываний
static constexpr unsigned long TEST_NOW_MS = 100000;

// Метрики со свежими отметками времени и значениями в пределах нормы:
// ни одна из девяти проверок на них не срабатывает
static CanMetrics fresh_metrics()
{
    CanMetrics m       = {};
    const uint32_t now = millis();

    m.engine_coolant_ts    = now;
    m.engine_oil_ts        = now;
    m.radiator_coolant_ts  = now;
    m.engine_rpm_ts        = now;
    m.turbo_boost_volt_ts  = now;
    m.oil_pressure_volt_ts = now;
    m.battery_voltage_ts   = now;
    m.cvt_temp_ts          = now;

    m.engine_coolant    = 90.0f;
    m.engine_oil        = 90.0f;
    m.radiator_coolant  = 50.0f;
    m.engine_rpm        = 800.0f;
    m.oil_pressure_volt = 2.0f;
    m.battery_voltage   = 13.5f;
    m.cvt_temp          = 70.0f;

    return m;
}

// Те же метрики, но с перегретым маслом ДВС — срабатывает ровно E01.
// Отдельная функция нужна там, где update() вызывается повторно после сдвига
// времени: отметки *_ts должны быть свежими на момент каждого вызова
static CanMetrics fresh_metrics_with_oil_high()
{
    CanMetrics m = fresh_metrics();
    m.engine_oil = 99.0f;
    return m;
}

// Конфиг проверок, где E01 переведена в однократный режим (enabled = false)
static constexpr const char *E01_ONCE_JSON =
    "{\"E01\":{\"enabled\":false,\"param1\":98.0,\"param2\":0.0,\"param3\":0.0}}";

void setUp(void)
{
    mock_fs_reset();
    buzzer_alert_count = 0;
    mock_set_millis(TEST_NOW_MS);
}

void tearDown(void) {}

// ── Базовое поведение ────────────────────────────────────────────────────────

static void test_normal_metrics_do_not_trigger(void)
{
    AlertManager am;
    am.update(fresh_metrics());

    TEST_ASSERT_FALSE(am.has_active_alert());
    TEST_ASSERT_EQUAL_UINT8(0, am.log_count());
    TEST_ASSERT_EQUAL_UINT32(0, buzzer_alert_count);
}

static void test_zero_timestamp_skips_check(void)
{
    // Метрика запредельная, но ни одного кадра по ней не приходило —
    // проверять нечего
    CanMetrics m = {};
    m.engine_oil = 200.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_FALSE(am.has_active_alert());
    TEST_ASSERT_EQUAL_UINT8(0, am.log_count());
}

// ── Пороги отдельных проверок ────────────────────────────────────────────────

static void test_engine_oil_high_triggers_e01(void)
{
    CanMetrics m = fresh_metrics();
    m.engine_oil = 99.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_TRUE(am.has_active_alert());
    TEST_ASSERT_EQUAL_STRING("E01", am.active_code());
    TEST_ASSERT_EQUAL_UINT8(1, am.log_count());
    TEST_ASSERT_EQUAL_UINT32(1, buzzer_alert_count);
}

static void test_threshold_is_strict(void)
{
    // Порог E01 — 98.0, сравнение строгое: ровно на пороге тревоги нет
    CanMetrics m = fresh_metrics();
    m.engine_oil = 98.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_FALSE(am.has_active_alert());
}

static void test_engine_coolant_high_triggers_e02(void)
{
    CanMetrics m     = fresh_metrics();
    m.engine_coolant = 97.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E02", am.active_code());
}

static void test_radiator_high_triggers_e03(void)
{
    CanMetrics m       = fresh_metrics();
    m.radiator_coolant = 91.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E03", am.active_code());
}

static void test_cvt_high_triggers_e04(void)
{
    CanMetrics m = fresh_metrics();
    m.cvt_temp   = 101.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E04", am.active_code());
}

static void test_rpm_overspeed_triggers_e05(void)
{
    CanMetrics m = fresh_metrics();
    m.engine_rpm = 6600.0f;
    // На высоких оборотах порог давления масла другой — поднимаем напряжение,
    // иначе заодно сработает E08 и перебьет активный код
    m.oil_pressure_volt = 3.5f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E05", am.active_code());
}

static void test_battery_low_triggers_e06(void)
{
    CanMetrics m      = fresh_metrics();
    m.battery_voltage = 11.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E06", am.active_code());
}

static void test_battery_high_triggers_e07(void)
{
    CanMetrics m      = fresh_metrics();
    m.battery_voltage = 15.5f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E07", am.active_code());
}

// ── E08: порог давления масла зависит от оборотов ────────────────────────────

static void test_oil_pressure_low_below_rpm_threshold(void)
{
    // Ниже 3000 об/мин минимум — 1.4 В
    CanMetrics m        = fresh_metrics();
    m.engine_rpm        = 2000.0f;
    m.oil_pressure_volt = 1.3f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E08", am.active_code());
}

static void test_oil_pressure_ok_below_rpm_threshold(void)
{
    CanMetrics m        = fresh_metrics();
    m.engine_rpm        = 2000.0f;
    m.oil_pressure_volt = 1.5f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_FALSE(am.has_active_alert());
}

static void test_oil_pressure_uses_high_threshold_above_rpm_limit(void)
{
    // Выше 3000 об/мин минимум уже 2.9 В — те же 2.0 В теперь авария
    CanMetrics m        = fresh_metrics();
    m.engine_rpm        = 4000.0f;
    m.oil_pressure_volt = 2.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E08", am.active_code());
}

static void test_oil_pressure_ok_above_rpm_limit(void)
{
    CanMetrics m        = fresh_metrics();
    m.engine_rpm        = 4000.0f;
    m.oil_pressure_volt = 3.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_FALSE(am.has_active_alert());
}

static void test_oil_pressure_check_skipped_when_engine_stopped(void)
{
    // Двигатель заглушен: давления нет и быть не должно
    CanMetrics m        = fresh_metrics();
    m.engine_rpm        = 0.0f;
    m.oil_pressure_volt = 0.1f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_FALSE(am.has_active_alert());
}

// ── E09: дельта температур масла и антифриза ─────────────────────────────────

static void test_oil_coolant_delta_triggers_e09(void)
{
    CanMetrics m     = fresh_metrics();
    m.engine_oil     = 95.0f;
    m.engine_coolant = 78.0f; // дельта 17 при пороге 14

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E09", am.active_code());
}

static void test_negative_delta_does_not_trigger(void)
{
    // Масло холоднее антифриза — это норма на прогреве
    CanMetrics m     = fresh_metrics();
    m.engine_oil     = 60.0f;
    m.engine_coolant = 90.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_FALSE(am.has_active_alert());
}

static void test_last_triggered_check_wins(void)
{
    // Сработали и E01, и E09 — активным остается последний по порядку проверок,
    // но в журнал попадают оба
    CanMetrics m     = fresh_metrics();
    m.engine_oil     = 99.0f;
    m.engine_coolant = 80.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E09", am.active_code());
    TEST_ASSERT_EQUAL_UINT8(2, am.log_count());
}

// ── Режимы повтора ───────────────────────────────────────────────────────────

static void test_retrigger_is_debounced(void)
{
    CanMetrics m = fresh_metrics();
    m.engine_oil = 99.0f;

    AlertManager am;
    am.update(m);
    TEST_ASSERT_EQUAL_UINT32(1, buzzer_alert_count);

    // Внутри окна антидребезга повторов нет
    mock_advance_millis(ALERT_RETRIGGER_MS - 1);
    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT32(1, buzzer_alert_count);

    // Ровно на границе окно уже закрыто
    mock_advance_millis(1);
    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT32(2, buzzer_alert_count);
}

static void test_once_mode_triggers_only_once(void)
{
    AlertManager am;
    // enabled = false переводит проверку в режим «один раз, пока код в журнале»
    TEST_ASSERT_TRUE(am.checks_from_json(E01_ONCE_JSON));

    CanMetrics m = fresh_metrics();
    m.engine_oil = 99.0f;

    am.update(m);
    TEST_ASSERT_EQUAL_UINT32(1, buzzer_alert_count);

    mock_advance_millis(ALERT_RETRIGGER_MS * 10);
    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT32(1, buzzer_alert_count);
}

static void test_once_mode_alerts_again_after_clear_log(void)
{
    // Однократность держится на записи в журнале, а не на флаге сессии:
    // стерли код из памяти — при сохранившейся неисправности алерт повторяется
    AlertManager am;
    TEST_ASSERT_TRUE(am.checks_from_json(E01_ONCE_JSON));

    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT32(1, buzzer_alert_count);

    TEST_ASSERT_TRUE(am.clear_log());

    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT32(2, buzzer_alert_count);
    TEST_ASSERT_EQUAL_UINT8(1, am.log_count());
    TEST_ASSERT_EQUAL_STRING("E01", am.active_code());
}

static void test_once_mode_stays_silent_while_code_in_log(void)
{
    // Перезагрузка МК больше не «прощает» код: он загружается из /alerts.json,
    // значит однократная проверка молчит, пока журнал не очистили
    mock_fs_files[CHECKS_CONFIG_FILE] = E01_ONCE_JSON;
    mock_fs_files[ALERTS_LOG_FILE] =
        "[{\"code\":\"E01\",\"description\":\"Масло\",\"count\":1}]";

    AlertManager am;
    am.init();

    am.update(fresh_metrics_with_oil_high());

    TEST_ASSERT_EQUAL_UINT32(0, buzzer_alert_count);
    TEST_ASSERT_FALSE(am.has_active_alert());
}

static void test_clear_log_resets_retrigger_debounce(void)
{
    // В режиме повтора очистка журнала тоже обнуляет антидребезг —
    // ждать конца 15-секундного окна после очистки не нужно
    AlertManager am;
    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT32(1, buzzer_alert_count);

    TEST_ASSERT_TRUE(am.clear_log());

    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT32(2, buzzer_alert_count);
}

static void test_active_alert_expires(void)
{
    CanMetrics m = fresh_metrics();
    m.engine_oil = 99.0f;

    AlertManager am;
    am.update(m);
    TEST_ASSERT_TRUE(am.has_active_alert());

    mock_advance_millis(ALERT_DISPLAY_MS - 1);
    TEST_ASSERT_TRUE(am.has_active_alert());

    mock_advance_millis(1);
    TEST_ASSERT_FALSE(am.has_active_alert());
}

static void test_active_display_name_matches_definition(void)
{
    CanMetrics m = fresh_metrics();
    m.engine_oil = 99.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("ENGINE OIL\nTemperature\nHIGH", am.active_display_name());
    TEST_ASSERT_EQUAL_STRING(AlertManager::CHECK_DEFS[0].description, am.active_description());
}

// ── Журнал ───────────────────────────────────────────────────────────────────

static void test_log_deduplicates_by_code(void)
{
    AlertManager am;
    am.update(fresh_metrics_with_oil_high());

    mock_advance_millis(ALERT_RETRIGGER_MS);
    am.update(fresh_metrics_with_oil_high());

    TEST_ASSERT_EQUAL_UINT8(1, am.log_count());

    // Счетчик срабатываний растет внутри той же записи
    String json = am.log_to_json();
    TEST_ASSERT_TRUE(json.find("\"count\":2") != std::string::npos);
    TEST_ASSERT_TRUE(json.find("\"code\":\"E01\"") != std::string::npos);
}

static void test_log_records_distinct_codes(void)
{
    CanMetrics m      = fresh_metrics();
    m.engine_oil      = 99.0f;
    m.battery_voltage = 11.0f;

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_UINT8(2, am.log_count());
}

static void test_log_is_persisted(void)
{
    AlertManager am;
    am.update(fresh_metrics_with_oil_high());

    TEST_ASSERT_TRUE(mock_fs_files.count(ALERTS_LOG_FILE) != 0);
    TEST_ASSERT_TRUE(mock_fs_files[ALERTS_LOG_FILE].find("E01") != std::string::npos);
    // Временный файл атомарной записи не остается на флеше
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(std::string(ALERTS_LOG_FILE) + ".tmp"));
}

static void test_clear_log_empties_log_and_file(void)
{
    AlertManager am;
    am.update(fresh_metrics_with_oil_high());
    TEST_ASSERT_EQUAL_UINT8(1, am.log_count());

    TEST_ASSERT_TRUE(am.clear_log());

    TEST_ASSERT_EQUAL_UINT8(0, am.log_count());
    TEST_ASSERT_EQUAL_STRING("", am.active_code());
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(ALERTS_LOG_FILE));
    TEST_ASSERT_EQUAL_STRING("[]", am.log_to_json().c_str());
}

// ── Конфиг проверок ──────────────────────────────────────────────────────────

static void test_checks_to_json_exposes_defaults(void)
{
    AlertManager am;

    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, am.checks_to_json()));

    TEST_ASSERT_EQUAL_FLOAT(98.0f, doc["E01"]["param1"].as<float>());
    TEST_ASSERT_TRUE(doc["E01"]["enabled"].as<bool>());
    TEST_ASSERT_EQUAL_FLOAT(3000.0f, doc["E08"]["param1"].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(1.4f, doc["E08"]["param2"].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(2.9f, doc["E08"]["param3"].as<float>());
}

static void test_checks_from_json_changes_threshold(void)
{
    AlertManager am;
    TEST_ASSERT_TRUE(am.checks_from_json(
        "{\"E01\":{\"enabled\":true,\"param1\":80.0,\"param2\":0.0,\"param3\":0.0}}"));

    CanMetrics m = fresh_metrics();
    m.engine_oil = 85.0f; // выше нового порога, ниже заводского

    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E01", am.active_code());
}

static void test_checks_from_json_rejects_invalid(void)
{
    AlertManager am;
    TEST_ASSERT_FALSE(am.checks_from_json("{ это не json"));
}

static void test_checks_from_json_is_persisted(void)
{
    AlertManager am;
    am.checks_from_json(
        "{\"E01\":{\"enabled\":true,\"param1\":80.0,\"param2\":0.0,\"param3\":0.0}}");

    TEST_ASSERT_TRUE(mock_fs_files.count(CHECKS_CONFIG_FILE) != 0);
    TEST_ASSERT_TRUE(mock_fs_files[CHECKS_CONFIG_FILE].find("80") != std::string::npos);
}

// ── Инициализация и миграция файлов ──────────────────────────────────────────

static void test_init_creates_checks_file_when_missing(void)
{
    AlertManager am;
    TEST_ASSERT_TRUE(am.init());

    TEST_ASSERT_TRUE(mock_fs_files.count(CHECKS_CONFIG_FILE) != 0);
    TEST_ASSERT_TRUE(mock_fs_files[CHECKS_CONFIG_FILE].find("E09") != std::string::npos);
}

static void test_init_backfills_missing_checks(void)
{
    // Файл от старой прошивки: в нем есть только E01, остальных проверок нет
    mock_fs_files[CHECKS_CONFIG_FILE] =
        "{\"E01\":{\"enabled\":true,\"param1\":42.0,\"param2\":0.0,\"param3\":0.0}}";

    AlertManager am;
    am.init();

    // Недостающие проверки дописаны заводскими значениями
    TEST_ASSERT_TRUE(mock_fs_files[CHECKS_CONFIG_FILE].find("E09") != std::string::npos);

    // При этом настройка пользователя из файла не потеряна
    CanMetrics m = fresh_metrics();
    m.engine_oil = 43.0f;
    am.update(m);
    TEST_ASSERT_EQUAL_STRING("E01", am.active_code());
}

static void test_init_loads_existing_log(void)
{
    mock_fs_files[ALERTS_LOG_FILE] =
        "[{\"code\":\"E05\",\"description\":\"Обороты\",\"count\":7}]";

    AlertManager am;
    am.init();

    TEST_ASSERT_EQUAL_UINT8(1, am.log_count());
    TEST_ASSERT_TRUE(am.log_to_json().find("\"count\":7") != std::string::npos);
}

static void test_init_survives_corrupted_log(void)
{
    mock_fs_files[ALERTS_LOG_FILE] = "{ обрезанный ф";

    AlertManager am;
    am.init();

    TEST_ASSERT_EQUAL_UINT8(0, am.log_count());
    // Битый файл удаляется, чтобы не мешать следующим запускам
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(ALERTS_LOG_FILE));
}

// ── Известный пробел ─────────────────────────────────────────────────────────

static void test_stale_metrics_still_trigger(void)
{
    // Проверки смотрят только на «*_ts != 0», но не на возраст значения.
    // Если ECU замолчал, последнее полученное значение продолжает считаться
    // актуальным и алертит спустя часы. Тест фиксирует текущее поведение —
    // когда протухание научатся отсекать, он должен упасть и напомнить о себе
    CanMetrics m = fresh_metrics();
    m.engine_oil = 99.0f;

    mock_advance_millis(3600000); // час без единого кадра

    AlertManager am;
    am.update(m);

    TEST_ASSERT_EQUAL_STRING("E01", am.active_code());
}

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_normal_metrics_do_not_trigger);
    RUN_TEST(test_zero_timestamp_skips_check);

    RUN_TEST(test_engine_oil_high_triggers_e01);
    RUN_TEST(test_threshold_is_strict);
    RUN_TEST(test_engine_coolant_high_triggers_e02);
    RUN_TEST(test_radiator_high_triggers_e03);
    RUN_TEST(test_cvt_high_triggers_e04);
    RUN_TEST(test_rpm_overspeed_triggers_e05);
    RUN_TEST(test_battery_low_triggers_e06);
    RUN_TEST(test_battery_high_triggers_e07);

    RUN_TEST(test_oil_pressure_low_below_rpm_threshold);
    RUN_TEST(test_oil_pressure_ok_below_rpm_threshold);
    RUN_TEST(test_oil_pressure_uses_high_threshold_above_rpm_limit);
    RUN_TEST(test_oil_pressure_ok_above_rpm_limit);
    RUN_TEST(test_oil_pressure_check_skipped_when_engine_stopped);

    RUN_TEST(test_oil_coolant_delta_triggers_e09);
    RUN_TEST(test_negative_delta_does_not_trigger);
    RUN_TEST(test_last_triggered_check_wins);

    RUN_TEST(test_retrigger_is_debounced);
    RUN_TEST(test_once_mode_triggers_only_once);
    RUN_TEST(test_once_mode_alerts_again_after_clear_log);
    RUN_TEST(test_once_mode_stays_silent_while_code_in_log);
    RUN_TEST(test_clear_log_resets_retrigger_debounce);
    RUN_TEST(test_active_alert_expires);
    RUN_TEST(test_active_display_name_matches_definition);

    RUN_TEST(test_log_deduplicates_by_code);
    RUN_TEST(test_log_records_distinct_codes);
    RUN_TEST(test_log_is_persisted);
    RUN_TEST(test_clear_log_empties_log_and_file);

    RUN_TEST(test_checks_to_json_exposes_defaults);
    RUN_TEST(test_checks_from_json_changes_threshold);
    RUN_TEST(test_checks_from_json_rejects_invalid);
    RUN_TEST(test_checks_from_json_is_persisted);

    RUN_TEST(test_init_creates_checks_file_when_missing);
    RUN_TEST(test_init_backfills_missing_checks);
    RUN_TEST(test_init_loads_existing_log);
    RUN_TEST(test_init_survives_corrupted_log);

    RUN_TEST(test_stale_metrics_still_trigger);

    return UNITY_END();
}
