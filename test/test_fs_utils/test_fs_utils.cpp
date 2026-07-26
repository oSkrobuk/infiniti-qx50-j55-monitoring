#include <unity.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "BuzzerStub.h"
#include "FsUtils.h"

// Тесты атомарной записи JSON.
//
// Смысл fs_write_json_atomic в том, что пропадание питания посреди записи
// не должно оставлять на флеше обрезанный JSON. На живом железе такие сценарии
// воспроизводятся плохо, поэтому отказы флеша имитирует заглушка LittleFS

static constexpr const char *TEST_PATH     = "/test.json";
static constexpr const char *TEST_TMP_PATH = "/test.json.tmp";

// Документ, у которого известен точный результат сериализации
static void fill_doc(JsonDocument &doc)
{
    doc["alpha"] = 1;
    doc["beta"]  = "два";
}

void setUp(void)
{
    mock_fs_reset();
}

void tearDown(void) {}

// ── Нормальный путь ──────────────────────────────────────────────────────────

static void test_writes_document_to_target(void)
{
    JsonDocument doc;
    fill_doc(doc);

    TEST_ASSERT_TRUE(fs_write_json_atomic(TEST_PATH, doc));

    TEST_ASSERT_TRUE(mock_fs_files.count(TEST_PATH) != 0);
    TEST_ASSERT_EQUAL_STRING("{\"alpha\":1,\"beta\":\"два\"}", mock_fs_files[TEST_PATH].c_str());
}

static void test_leaves_no_temp_file(void)
{
    JsonDocument doc;
    fill_doc(doc);

    fs_write_json_atomic(TEST_PATH, doc);

    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(TEST_TMP_PATH));
}

static void test_overwrites_existing_file(void)
{
    mock_fs_files[TEST_PATH] = "{\"старое\":true}";

    JsonDocument doc;
    fill_doc(doc);

    TEST_ASSERT_TRUE(fs_write_json_atomic(TEST_PATH, doc));
    TEST_ASSERT_EQUAL_STRING("{\"alpha\":1,\"beta\":\"два\"}", mock_fs_files[TEST_PATH].c_str());
}

// ── Отказы флеша ─────────────────────────────────────────────────────────────

static void test_truncated_write_keeps_previous_file(void)
{
    mock_fs_files[TEST_PATH] = "{\"старое\":true}";

    // Места хватает лишь на несколько байт — запись оборвется на полуслове
    mock_fs_write_limit = 5;

    JsonDocument doc;
    fill_doc(doc);

    TEST_ASSERT_FALSE(fs_write_json_atomic(TEST_PATH, doc));

    // Ради этого все и затевалось: прежний файл цел, обрезка не доехала до цели
    TEST_ASSERT_EQUAL_STRING("{\"старое\":true}", mock_fs_files[TEST_PATH].c_str());
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(TEST_TMP_PATH));
}

static void test_fallback_when_rename_cannot_replace(void)
{
    mock_fs_files[TEST_PATH] = "{\"старое\":true}";

    // Реализация не умеет переименовывать поверх существующего файла —
    // для этого в FsUtils есть запасной путь «удалить цель и повторить»
    mock_fs_rename_fails_if_exists = true;

    JsonDocument doc;
    fill_doc(doc);

    TEST_ASSERT_TRUE(fs_write_json_atomic(TEST_PATH, doc));
    TEST_ASSERT_EQUAL_STRING("{\"alpha\":1,\"beta\":\"два\"}", mock_fs_files[TEST_PATH].c_str());
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(TEST_TMP_PATH));
}

static void test_failed_rename_reports_error_and_drops_target(void)
{
    mock_fs_files[TEST_PATH] = "{\"старое\":true}";

    mock_fs_rename_always_fails = true;

    JsonDocument doc;
    fill_doc(doc);

    TEST_ASSERT_FALSE(fs_write_json_atomic(TEST_PATH, doc));
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(TEST_TMP_PATH));

    // Запасной путь удаляет цель ПЕРЕД повторным переименованием, поэтому при
    // отказе обоих rename прежний файл теряется — данные не обрезаны, но их нет.
    // Тест фиксирует это как есть: сценарий требует rename, ломающийся дважды
    TEST_ASSERT_EQUAL_UINT32(0, mock_fs_files.count(TEST_PATH));
}

static void test_empty_document_is_written(void)
{
    JsonDocument doc;
    doc.to<JsonObject>();

    TEST_ASSERT_TRUE(fs_write_json_atomic(TEST_PATH, doc));
    TEST_ASSERT_EQUAL_STRING("{}", mock_fs_files[TEST_PATH].c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_writes_document_to_target);
    RUN_TEST(test_leaves_no_temp_file);
    RUN_TEST(test_overwrites_existing_file);

    RUN_TEST(test_truncated_write_keeps_previous_file);
    RUN_TEST(test_fallback_when_rename_cannot_replace);
    RUN_TEST(test_failed_rename_reports_error_and_drops_target);
    RUN_TEST(test_empty_document_is_written);

    return UNITY_END();
}
