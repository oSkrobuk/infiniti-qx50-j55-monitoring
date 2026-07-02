#include "ConfigManager.h"

#include <LittleFS.h>

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

    // Давление масла: минимум зависит от оборотов
    // при RPM < rpm_threshold допустимо min_low бар, при RPM >= threshold — min_high бар
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
    doc["battery"]["red_low"]   = 11.7f;
    doc["battery"]["green_min"] = 12.5f;
    doc["battery"]["green_max"] = 14.6f;
    doc["battery"]["red_high"]  = 14.9f;

    // Время опроса RPM: цветовые пороги (секунды)
    // ≤ green_max → зелёный; ≥ red_min → красный; между — плавный переход
    doc["poll_time"]["green_max"] = 0.2f;
    doc["poll_time"]["red_min"]   = 0.5f;

    // Системные параметры CAN-опроса
    // poll_interval_ms — пауза между отправками UDS-запросов (мс)
    // stale_ms         — через сколько мс без обновления значение считается устаревшим
    doc["system"]["poll_interval_ms"] = 30.0f;
    doc["system"]["stale_ms"]         = 1000.0f;

    // Настройки WiFi точки доступа (строки, не участвуют в числовом хеше)
    doc["wifi"]["ssid"]     = "QX50Monitoring";
    doc["wifi"]["password"] = "infiniti";
}

// ── Вспомогательные функции ──────────────────────────────────────────────────

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
    JsonDocument doc;
    build_defaults(doc);
    String s;
    serializeJson(doc, s);
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
    return data_[section][field] | 0.0f;
}

String ConfigManager::get_str(const char *section, const char *field) const
{
    JsonVariantConst v = data_[section][field];
    if (v.is<const char *>()) return String(v.as<const char *>());
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

    // Сравниваем хеш дефолтов из файла с текущим хешем дефолтов в коде
    // Если значения по умолчанию изменились — перезаписываем файл
    uint32_t file_hash    = doc["version"] | 0u;
    uint32_t current_hash = defaults_hash();

    if (file_hash != current_hash) {
        Serial.printf("[Config] Значения по умолчанию изменились (hash %08X -> %08X), сбрасываем конфиг\r\n",
                      file_hash, current_hash);
        return save_to_file();
    }

    // Загружаем params поверх дефолтов: неизвестные поля просто игнорируются
    // Строки (wifi ssid/password) сохраняются как строки, числа — как float
    JsonObjectConst params = doc["params"].as<JsonObjectConst>();
    for (JsonPairConst section : params) {
        JsonObjectConst fields = section.value().as<JsonObjectConst>();
        for (JsonPairConst field : fields) {
            JsonVariantConst v = field.value();
            if (v.is<const char *>()) {
                data_[section.key()][field.key()] = v.as<const char *>();
            } else {
                data_[section.key()][field.key()] = v.as<float>();
            }
        }
    }

    Serial.println("[Config] Конфигурация загружена");
    return true;
}

bool ConfigManager::save_to_file()
{
    if (!ensure_mounted()) return false;

    File f = LittleFS.open("/config.json", "w");
    if (!f) {
        Serial.println("[Config] ОШИБКА: не удалось открыть файл для записи");
        return false;
    }

    JsonDocument doc;
    doc["version"] = defaults_hash();
    doc["params"]  = data_;

    if (serializeJson(doc, f) == 0) {
        Serial.println("[Config] ОШИБКА: не удалось записать JSON");
        f.close();
        return false;
    }

    f.close();
    Serial.println("[Config] Конфигурация сохранена");
    return true;
}

bool ConfigManager::reset_to_defaults()
{
    apply_defaults();
    Serial.println("[Config] Сброс к значениям по умолчанию");
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

    // Обновляем только те поля, которые пришли; остальные остаются как есть
    // Строки (wifi ssid/password) сохраняются как строки, числа — как float
    JsonObjectConst root = doc.as<JsonObjectConst>();
    for (JsonPairConst section : root) {
        JsonObjectConst fields = section.value().as<JsonObjectConst>();
        for (JsonPairConst field : fields) {
            JsonVariantConst v = field.value();
            if (v.is<const char *>()) {
                data_[section.key()][field.key()] = v.as<const char *>();
            } else {
                data_[section.key()][field.key()] = v.as<float>();
            }
        }
    }

    return save_to_file();
}
