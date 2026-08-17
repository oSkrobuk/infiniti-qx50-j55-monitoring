#include <unity.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "BuzzerStub.h"
#include "ConfigManager.h"

// Тесты конфигурации: заводские значения, откат при отсутствующих полях,
// разделение чисел и строк, а также миграция файла при смене дефолтов.
//
// Последнее важнее всего: в файл пишется CRC32 от заводских значений, и любое
// изменение build_defaults() обнуляет настройки на устройстве пользователя

void setUp(void)
{
    mock_fs_reset();
}

void tearDown(void) {}

// ── Заводские значения ───────────────────────────────────────────────────────

static void test_defaults_applied_on_construction(void)
{
    ConfigManager cm;

    TEST_ASSERT_EQUAL_FLOAT(98.0f, cm.get("oil", "max"));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, cm.get("oil", "target"));
    TEST_ASSERT_EQUAL_FLOAT(4500.0f, cm.get("rpm", "red_start"));
    TEST_ASSERT_EQUAL_FLOAT(3000.0f, cm.get("oil_pressure", "rpm_threshold"));
    TEST_ASSERT_EQUAL_FLOAT(1000.0f, cm.get("system", "stale_ms"));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, cm.get("system", "brightness_percent"));
}

static void test_unknown_field_returns_zero(void)
{
    ConfigManager cm;

    TEST_ASSERT_EQUAL_FLOAT(0.0f, cm.get("oil", "нет_такого_поля"));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, cm.get("нет_такой_секции", "max"));
}

static void test_string_defaults(void)
{
    ConfigManager cm;

    TEST_ASSERT_EQUAL_STRING("QX50Monitoring", cm.get_str("wifi", "ssid").c_str());
    TEST_ASSERT_EQUAL_STRING("infiniti", cm.get_str("wifi", "password").c_str());
}

static void test_unknown_string_field_returns_empty(void)
{
    ConfigManager cm;

    TEST_ASSERT_EQUAL_STRING("", cm.get_str("wifi", "нет_такого_поля").c_str());
}

// ── Приём конфига из Web UI ──────────────────────────────────────────────────

static void test_from_json_updates_value(void)
{
    ConfigManager cm;
    TEST_ASSERT_TRUE(cm.from_json(
        "{\"oil\":{\"max\":105.0},\"system\":{\"brightness_percent\":60}}"));

    TEST_ASSERT_EQUAL_FLOAT(105.0f, cm.get("oil", "max"));
    TEST_ASSERT_EQUAL_FLOAT(60.0f, cm.get("system", "brightness_percent"));
}

static void test_from_json_keeps_other_fields(void)
{
    ConfigManager cm;
    cm.from_json("{\"oil\":{\"max\":105.0}}");

    // Пришло только одно поле — остальные обязаны остаться прежними
    TEST_ASSERT_EQUAL_FLOAT(90.0f, cm.get("oil", "target"));
    TEST_ASSERT_EQUAL_FLOAT(50.0f, cm.get("oil", "min"));
    TEST_ASSERT_EQUAL_FLOAT(93.0f, cm.get("coolant", "max"));
}

static void test_from_json_rejects_invalid(void)
{
    ConfigManager cm;

    TEST_ASSERT_FALSE(cm.from_json("{ это не json"));
    TEST_ASSERT_EQUAL_FLOAT(98.0f, cm.get("oil", "max"));
}

static void test_from_json_keeps_strings_as_strings(void)
{
    ConfigManager cm;
    TEST_ASSERT_TRUE(cm.from_json("{\"wifi\":{\"ssid\":\"МояСеть\",\"password\":\"12345678\"}}"));

    TEST_ASSERT_EQUAL_STRING("МояСеть", cm.get_str("wifi", "ssid").c_str());

    // Строка не должна была превратиться в 0 при сериализации обратно
    TEST_ASSERT_TRUE(cm.to_json().find("\"ssid\":\"МояСеть\"") != std::string::npos);
}

static void test_from_json_persists_to_file(void)
{
    ConfigManager cm;
    cm.from_json("{\"oil\":{\"max\":105.0}}");

    TEST_ASSERT_TRUE(mock_fs_files.count("/config.json") != 0);
    // Атомарная запись не оставляет за собой временный файл
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count("/config.json.tmp"));
}

// ── Файл конфига ─────────────────────────────────────────────────────────────

static void test_save_and_load_round_trip(void)
{
    ConfigManager writer;
    writer.from_json("{\"oil\":{\"max\":105.0},\"wifi\":{\"ssid\":\"Round\"}}");

    ConfigManager reader;
    TEST_ASSERT_TRUE(reader.load_from_file());

    TEST_ASSERT_EQUAL_FLOAT(105.0f, reader.get("oil", "max"));
    TEST_ASSERT_EQUAL_STRING("Round", reader.get_str("wifi", "ssid").c_str());
}

static void test_load_creates_file_when_missing(void)
{
    ConfigManager cm;
    TEST_ASSERT_TRUE(cm.load_from_file());

    TEST_ASSERT_TRUE(mock_fs_files.count("/config.json") != 0);
}

static void test_config_file_stores_defaults_hash(void)
{
    ConfigManager cm;
    cm.save_to_file();

    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, mock_fs_files["/config.json"]));
    TEST_ASSERT_TRUE(doc["version"].is<uint32_t>());
    TEST_ASSERT_NOT_EQUAL(0u, doc["version"].as<uint32_t>());
    TEST_ASSERT_TRUE(doc["params"]["oil"]["max"].is<float>());
}

static void test_load_resets_config_when_defaults_changed(void)
{
    // Файл от прошивки с другими заводскими значениями: хеш не совпадет
    mock_fs_files["/config.json"] =
        "{\"version\":1,\"params\":{\"oil\":{\"max\":105.0}}}";

    ConfigManager cm;
    TEST_ASSERT_TRUE(cm.load_from_file());

    // Пользовательское значение намеренно отброшено — это цена смены дефолтов
    TEST_ASSERT_EQUAL_FLOAT(98.0f, cm.get("oil", "max"));

    // Файл переписан с актуальным хешем: следующая загрузка уже не сбросится
    ConfigManager reloaded;
    reloaded.from_json("{\"oil\":{\"max\":103.0}}");
    ConfigManager after;
    after.load_from_file();
    TEST_ASSERT_EQUAL_FLOAT(103.0f, after.get("oil", "max"));
}

static void test_load_fails_on_corrupted_file(void)
{
    mock_fs_files["/config.json"] = "{ обрезанный ф";

    ConfigManager cm;
    TEST_ASSERT_FALSE(cm.load_from_file());

    // Пороги при этом остаются заводскими, а не нулевыми
    TEST_ASSERT_EQUAL_FLOAT(98.0f, cm.get("oil", "max"));
}

static void test_reset_to_defaults(void)
{
    ConfigManager cm;
    cm.from_json("{\"oil\":{\"max\":105.0}}");
    TEST_ASSERT_EQUAL_FLOAT(105.0f, cm.get("oil", "max"));

    TEST_ASSERT_TRUE(cm.reset_to_defaults());
    TEST_ASSERT_EQUAL_FLOAT(98.0f, cm.get("oil", "max"));
    TEST_ASSERT_EQUAL_STRING("QX50Monitoring", cm.get_str("wifi", "ssid").c_str());
}

static void test_to_json_contains_all_sections(void)
{
    ConfigManager cm;

    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, cm.to_json()));

    TEST_ASSERT_TRUE(doc["oil"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["coolant"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["radiator"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["transmission"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["rpm"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["oil_pressure"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["boost"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["battery"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["poll_time"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["system"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["wifi"].is<JsonObject>());
}

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_defaults_applied_on_construction);
    RUN_TEST(test_unknown_field_returns_zero);
    RUN_TEST(test_string_defaults);
    RUN_TEST(test_unknown_string_field_returns_empty);

    RUN_TEST(test_from_json_updates_value);
    RUN_TEST(test_from_json_keeps_other_fields);
    RUN_TEST(test_from_json_rejects_invalid);
    RUN_TEST(test_from_json_keeps_strings_as_strings);
    RUN_TEST(test_from_json_persists_to_file);

    RUN_TEST(test_save_and_load_round_trip);
    RUN_TEST(test_load_creates_file_when_missing);
    RUN_TEST(test_config_file_stores_defaults_hash);
    RUN_TEST(test_load_resets_config_when_defaults_changed);
    RUN_TEST(test_load_fails_on_corrupted_file);
    RUN_TEST(test_reset_to_defaults);
    RUN_TEST(test_to_json_contains_all_sections);

    return UNITY_END();
}
