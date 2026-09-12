#include "ConfigManager.h"

#include <math.h>

#include <LittleFS.h>

#include "FsUtils.h"
#include "WifiCredentials.h"

ConfigManager config;

static bool s_fs_mounted = false;

// ── Значения по умолчанию ────────────────────────────────────────────────────
//
// Добавить новый датчик/секцию с любой структурой полей — одна лямбда ниже
// Температурные датчики: min / target / max
// Другие типы могут иметь любые поля: warn, crit, idle, ...
//
static void build_defaults(JsonDocument &doc)
{
    // Температурные датчики
    doc["oil"]["min"]            = 50.0f;
    doc["oil"]["target"]         = 90.0f;
    doc["oil"]["max"]            = 98.0f;

    doc["coolant"]["min"]        = 50.0f;
    doc["coolant"]["target"]     = 90.0f;
    doc["coolant"]["max"]        = 93.0f;

    doc["radiator"]["min"]       =  0.0f;
    doc["radiator"]["target"]    = 50.0f;
    doc["radiator"]["max"]       = 90.0f;

    doc["transmission"]["min"]    = 50.0f;
    doc["transmission"]["target"] = 80.0f;
    doc["transmission"]["max"]    = 98.0f;

    // Обороты двигателя: три порога цветовой зоны
    // синий < green_start < зелёный < green_end < жёлтый..красный < red_start
    doc["rpm"]["green_start"] = 1000.0f;
    doc["rpm"]["green_end"]   = 3500.0f;
    doc["rpm"]["red_start"]   = 4500.0f;

    // Напряжение датчика давления масла: минимум зависит от оборотов
    // при RPM < rpm_threshold допустимо min_low В, при RPM >= threshold — min_high В
    doc["oil_pressure"]["rpm_threshold"] = 3000.0f;
    doc["oil_pressure"]["min_low"]       = 1.45f;
    doc["oil_pressure"]["min_high"]      = 3.1f;

    // Наддув: цветовые пороги
    // ≤ blue_max → синий; ≥ green_min → зелёный; между — плавно через жёлтый
    doc["boost"]["blue_max"]  = 1.3f;
    doc["boost"]["green_min"] = 1.58f;

    // Бортовая сеть: цветовые пороги напряжения (Вольты)
    // < red_low  → красный; red_low..green_min → жёлтый; green_min..green_max → зелёный
    // green_max..red_high → жёлтый; > red_high → красный
    doc["battery"]["red_low"]   = 11.5f;
    doc["battery"]["green_min"] = 12.0f;
    doc["battery"]["green_max"] = 14.6f;
    doc["battery"]["red_high"]  = 14.9f;

    // Период обновления RPM: цветовые пороги (секунды)
    // ≤ green_max → зелёный; ≥ red_min → красный; между — плавный переход
    doc["poll_time"]["green_max"] = 0.2f;
    doc["poll_time"]["red_min"]   = 0.5f;

    // Системные параметры CAN-опроса и дисплея
    // poll_interval_ms          — пауза между отправками основных UDS-запросов (мс)
    // obd_request_spacing_ms    — пауза между OBD PID внутри секундного пакета (мс)
    // stale_ms                  — через сколько мс без обновления значение считается устаревшим
    // brightness_percent        — яркость управляемой подсветки дисплея в процентах
    doc["system"]["poll_interval_ms"]        = 30.0f;
    doc["system"]["obd_request_spacing_ms"] = 5.0f;
    doc["system"]["stale_ms"]                = 1000.0f;
    doc["system"]["brightness_percent"]      = 100.0f;

    // Настройки WiFi точки доступа (строки, не участвуют в числовом хеше)
    doc["wifi"]["ssid"]     = WIFI_DEFAULT_SSID;
    doc["wifi"]["password"] = WIFI_DEFAULT_PASSWORD;
}

// ── Вспомогательные функции ──────────────────────────────────────────────────

// Документ с заводскими значениями — строится один раз при первом обращении.
// Нужен как запасной источник для get()/get_str(), если поля нет в текущем конфиге:
// без него повреждённый или неполный файл давал бы нулевые пороги
static const JsonDocument &defaults_doc()
{
    static JsonDocument s_defaults;
    static bool         s_built = false;

    if (!s_built) {
        build_defaults(s_defaults);
        s_built = true;
    }
    return s_defaults;
}

static bool number_in_range(JsonObjectConst root, const char *section, const char *field,
                            float minimum, float maximum)
{
    JsonVariantConst value = root[section][field];
    if (value.is<bool>() || !value.is<float>()) return false;

    const float number = value.as<float>();
    return isfinite(number) && number >= minimum && number <= maximum;
}

static bool ordered(JsonObjectConst root, const char *section,
                    const char *first, const char *second)
{
    return root[section][first].as<float>() < root[section][second].as<float>();
}

static bool config_values_valid(JsonObjectConst root)
{
    static const char *temperature_sections[] = {"oil", "coolant", "radiator", "transmission"};
    for (const char *section : temperature_sections) {
        if (!number_in_range(root, section, "min", -100.0f, 250.0f) ||
            !number_in_range(root, section, "target", -100.0f, 250.0f) ||
            !number_in_range(root, section, "max", -100.0f, 250.0f) ||
            !ordered(root, section, "min", "target") ||
            !ordered(root, section, "target", "max")) return false;
    }

    if (!number_in_range(root, "rpm", "green_start", 0.0f, 10000.0f) ||
        !number_in_range(root, "rpm", "green_end", 0.0f, 10000.0f) ||
        !number_in_range(root, "rpm", "red_start", 0.0f, 10000.0f) ||
        !ordered(root, "rpm", "green_start", "green_end") ||
        !ordered(root, "rpm", "green_end", "red_start")) return false;

    if (!number_in_range(root, "oil_pressure", "rpm_threshold", 1.0f, 10000.0f) ||
        !number_in_range(root, "oil_pressure", "min_low", 0.01f, 5.0f) ||
        !number_in_range(root, "oil_pressure", "min_high", 0.01f, 5.0f) ||
        !ordered(root, "oil_pressure", "min_low", "min_high")) return false;

    if (!number_in_range(root, "boost", "blue_max", 0.0f, 5.0f) ||
        !number_in_range(root, "boost", "green_min", 0.0f, 5.0f) ||
        !ordered(root, "boost", "blue_max", "green_min")) return false;

    if (!number_in_range(root, "battery", "red_low", 0.0f, 32.0f) ||
        !number_in_range(root, "battery", "green_min", 0.0f, 32.0f) ||
        !number_in_range(root, "battery", "green_max", 0.0f, 32.0f) ||
        !number_in_range(root, "battery", "red_high", 0.0f, 32.0f) ||
        !ordered(root, "battery", "red_low", "green_min") ||
        !ordered(root, "battery", "green_min", "green_max") ||
        !ordered(root, "battery", "green_max", "red_high")) return false;

    if (!number_in_range(root, "poll_time", "green_max", 0.001f, 60.0f) ||
        !number_in_range(root, "poll_time", "red_min", 0.001f, 60.0f) ||
        !ordered(root, "poll_time", "green_max", "red_min")) return false;

    if (!number_in_range(root, "system", "poll_interval_ms", 10.0f, 1000.0f) ||
        !number_in_range(root, "system", "obd_request_spacing_ms", 1.0f, 100.0f) ||
        !number_in_range(root, "system", "stale_ms", 100.0f, 3600000.0f) ||
        !number_in_range(root, "system", "brightness_percent", 10.0f, 100.0f) ||
        !ordered(root, "system", "poll_interval_ms", "stale_ms")) return false;

    const float brightness = root["system"]["brightness_percent"].as<float>();
    if (fabsf(brightness / 10.0f - roundf(brightness / 10.0f)) > 0.0001f) return false;

    return wifi_credentials_validate(root["wifi"]["ssid"].as<const char *>(),
                                     root["wifi"]["password"].as<const char *>());
}

// CRC32 от строки — используется для автоматического определения
// изменения значений по умолчанию без ручного версионирования
static uint32_t crc32(const String &s)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (char c : s) {
        crc ^= static_cast<uint8_t>(c);
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
        }
    }
    return ~crc;
}

static uint32_t defaults_hash()
{
    String s;
    serializeJson(defaults_doc(), s);
    return crc32(s);
}

static bool ensure_mounted()
{
    if (s_fs_mounted) return true;

    if (!LittleFS.begin(false)) {
        Serial.println("[FS] Первый запуск, форматируем LittleFS...");
        if (!LittleFS.begin(true)) {
            Serial.println("[FS] ОШИБКА: не удалось смонтировать LittleFS!");
            return false;
        }
    }
    s_fs_mounted = true;
    Serial.println("[FS] LittleFS смонтирован");
    return true;
}

// ── ConfigManager ─────────────────────────────────────────────────────────────

ConfigManager::ConfigManager()
{
    apply_defaults();
}

void ConfigManager::apply_defaults()
{
    data_.clear();
    build_defaults(data_);
}

float ConfigManager::get(const char *section, const char *field) const
{
    JsonVariantConst v = data_[section][field];
    if (v.is<float>()) return v.as<float>();

    // Поля нет в текущем конфиге — откатываемся на заводское значение,
    // иначе порог молча стал бы нулём и сломал цветовые зоны
    return defaults_doc()[section][field] | 0.0f;
}

String ConfigManager::get_str(const char *section, const char *field) const
{
    JsonVariantConst v = data_[section][field];
    if (v.is<const char *>()) return String(v.as<const char *>());

    // Поля нет в текущем конфиге — откатываемся на заводское значение
    JsonVariantConst d = defaults_doc()[section][field];
    if (d.is<const char *>()) return String(d.as<const char *>());

    return String("");
}

bool ConfigManager::init()
{
    if (!ensure_mounted()) return false;
    return load_from_file();
}

bool ConfigManager::load_from_file()
{
    if (!ensure_mounted()) return false;

    if (!LittleFS.exists("/config.json")) {
        Serial.println("[Config] Файл не найден, сохраняем значения по умолчанию");
        return save_to_file();
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) {
        Serial.println("[Config] ОШИБКА: не удалось открыть файл для чтения");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Config] ОШИБКА парсинга JSON: %s\r\n", err.c_str());
        return false;
    }

    // Хеш нужен только для обнаружения миграции, но не для сброса настроек
    uint32_t file_hash    = doc["version"] | 0u;
    uint32_t current_hash = defaults_hash();

    // Начинаем с актуальных значений по умолчанию и переносим из файла каждое
    // известное поле отдельно. Новые поля получают дефолт, удаленные игнорируются,
    // а сохраненные пользовательские значения не теряются при обновлении прошивки
    apply_defaults();
    JsonObjectConst params = doc["params"].as<JsonObjectConst>();
    JsonObjectConst defaults = defaults_doc().as<JsonObjectConst>();
    for (JsonPairConst default_section : defaults) {
        JsonObjectConst default_fields = default_section.value().as<JsonObjectConst>();
        for (JsonPairConst default_field : default_fields) {
            JsonVariantConst saved = params[default_section.key()][default_field.key()];
            if (saved.isNull()) continue;

            JsonVariantConst fallback = default_field.value();
            if (fallback.is<const char *>() && saved.is<const char *>()) {
                data_[default_section.key()][default_field.key()] = saved.as<const char *>();
            } else if (fallback.is<float>() && saved.is<float>()) {
                data_[default_section.key()][default_field.key()] = saved.as<float>();
            }
        }
    }

    WifiCredentialsError wifi_error;
    const bool wifi_valid = wifi_credentials_validate(get_str("wifi", "ssid"),
                                                       get_str("wifi", "password"), &wifi_error);
    if (!wifi_valid) {
        Serial.printf("[Config] Некорректные настройки WiFi (%s), восстановлены заводские\r\n",
                      wifi_credentials_error_name(wifi_error));
        data_["wifi"]["ssid"] = WIFI_DEFAULT_SSID;
        data_["wifi"]["password"] = WIFI_DEFAULT_PASSWORD;
    }

    if (!wifi_valid) {
        return save_to_file();
    }

    if (file_hash != current_hash) {
        Serial.printf("[Config] Миграция настроек (hash %08X -> %08X)\r\n", file_hash, current_hash);
        return save_to_file();
    }

    Serial.println("[Config] Конфигурация загружена");
    return true;
}

bool ConfigManager::save_to_file()
{
    if (!ensure_mounted()) return false;

    JsonDocument doc;
    doc["version"] = defaults_hash();
    doc["params"]  = data_;

    // Пишем атомарно: при пропадании питания прежний конфиг останется целым
    if (!fs_write_json_atomic("/config.json", doc)) {
        Serial.println("[Config] ОШИБКА: не удалось сохранить конфигурацию");
        return false;
    }

    Serial.println("[Config] Конфигурация сохранена");
    return true;
}

bool ConfigManager::reset_to_defaults()
{
    apply_defaults();
    Serial.println("[Config] Сброс к значениям по умолчанию");
    return save_to_file();
}

bool ConfigManager::set_wifi_credentials(const String &ssid, const String &password)
{
    if (!wifi_credentials_validate(ssid, password)) return false;

    data_["wifi"]["ssid"] = ssid;
    data_["wifi"]["password"] = password;
    return save_to_file();
}

bool ConfigManager::reset_wifi_to_defaults()
{
    data_["wifi"]["ssid"] = WIFI_DEFAULT_SSID;
    data_["wifi"]["password"] = WIFI_DEFAULT_PASSWORD;
    return save_to_file();
}

String ConfigManager::to_json() const
{
    String out;
    serializeJson(data_, out);
    return out;
}

bool ConfigManager::from_json(const String &json)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[Config] ОШИБКА парсинга входящего JSON: %s\r\n", err.c_str());
        return false;
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();
    if (root.isNull()) {
        Serial.println("[Config] ОШИБКА: корень конфигурации должен быть объектом");
        return false;
    }

    JsonDocument candidate;
    candidate.set(data_);
    JsonObjectConst schema = defaults_doc().as<JsonObjectConst>();

    // Разрешены только известные секции и поля с типом из заводской схемы
    for (JsonPairConst section : root) {
        JsonVariantConst schema_section = schema[section.key()];
        if (!schema_section.is<JsonObjectConst>() || !section.value().is<JsonObjectConst>()) {
            Serial.printf("[Config] ОШИБКА: неизвестная или неверная секция %s\r\n", section.key().c_str());
            return false;
        }

        JsonObjectConst fields = section.value().as<JsonObjectConst>();
        for (JsonPairConst field : fields) {
            JsonVariantConst expected = schema_section[field.key()];
            JsonVariantConst v = field.value();
            const bool string_field = expected.is<const char *>() && v.is<const char *>();
            const bool number_field = expected.is<float>() && !v.is<bool>() && v.is<float>();
            if (expected.isNull() || (!string_field && !number_field)) {
                Serial.printf("[Config] ОШИБКА: неизвестное поле или неверный тип %s.%s\r\n",
                              section.key().c_str(), field.key().c_str());
                return false;
            }

            candidate[section.key()][field.key()].set(v);
        }
    }

    if (!config_values_valid(candidate.as<JsonObjectConst>())) {
        Serial.println("[Config] ОШИБКА: значения выходят за допустимые пределы");
        return false;
    }

    data_.set(candidate);
    return save_to_file();
}
