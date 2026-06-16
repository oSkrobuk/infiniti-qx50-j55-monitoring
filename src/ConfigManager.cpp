#include "ConfigManager.h"
#include <LittleFS.h>

ConfigManager config;

static bool _fsMounted = false;

// ── Значения по умолчанию ────────────────────────────────────────────────────
//
// Добавить новый датчик/секцию с любой структурой полей — одна лямбда ниже.
// Температурные датчики: min / target / max
// Другие типы могут иметь любые поля: warn, crit, idle, ...
//
static void buildDefaults(JsonDocument &doc)
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

    doc["transmission"]["min"]   = 50.0f;
    doc["transmission"]["target"]= 80.0f;
    doc["transmission"]["max"]   = 100.0f;

    // Пример: датчик давления масла (другая структура полей)
    // doc["oil_pressure"]["warn"] = 1.5f;
    // doc["oil_pressure"]["crit"] = 0.8f;

    // Пример: обороты двигателя
    // doc["rpm"]["idle"]  = 800.0f;
    // doc["rpm"]["max"]   = 7000.0f;
}

// ── Вспомогательные функции ──────────────────────────────────────────────────

// CRC32 от строки — используется для автоматического определения
// изменения значений по умолчанию без ручного версионирования
static uint32_t crc32(const String &s)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (char c : s)
    {
        crc ^= (uint8_t)c;
        for (int b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return ~crc;
}

static uint32_t defaultsHash()
{
    JsonDocument doc;
    buildDefaults(doc);
    String s;
    serializeJson(doc, s);
    return crc32(s);
}

static bool ensureMounted()
{
    if (_fsMounted) return true;

    if (!LittleFS.begin(false))
    {
        Serial.println("[FS] Первый запуск, форматируем LittleFS...");
        if (!LittleFS.begin(true))
        {
            Serial.println("[FS] ОШИБКА: не удалось смонтировать LittleFS!");
            return false;
        }
    }
    _fsMounted = true;
    Serial.println("[FS] LittleFS смонтирован.");
    return true;
}

// ── ConfigManager ────────────────────────────────────────────────────────────

ConfigManager::ConfigManager()
{
    _applyDefaults();
}

void ConfigManager::_applyDefaults()
{
    _data.clear();
    buildDefaults(_data);
}

float ConfigManager::get(const char *section, const char *field) const
{
    return _data[section][field] | 0.0f;
}

bool ConfigManager::init()
{
    if (!ensureMounted()) return false;
    return loadFromFile();
}

bool ConfigManager::loadFromFile()
{
    if (!ensureMounted()) return false;

    if (!LittleFS.exists("/config.json"))
    {
        Serial.println("[Config] Файл не найден, сохраняем значения по умолчанию.");
        return saveToFile();
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f)
    {
        Serial.println("[Config] ОШИБКА: не удалось открыть файл для чтения.");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err)
    {
        Serial.printf("[Config] ОШИБКА парсинга JSON: %s\r\n", err.c_str());
        return false;
    }

    // Сравниваем хеш дефолтов из файла с текущим хешем дефолтов в коде.
    // Если значения по умолчанию изменились — перезаписываем файл.
    uint32_t fileHash    = doc["version"] | 0u;
    uint32_t currentHash = defaultsHash();

    if (fileHash != currentHash)
    {
        Serial.printf("[Config] Значения по умолчанию изменились (hash %08X -> %08X), сбрасываем конфиг.\r\n",
                      fileHash, currentHash);
        return saveToFile();
    }

    // Загружаем params поверх дефолтов: неизвестные поля просто игнорируются
    JsonObjectConst params = doc["params"].as<JsonObjectConst>();
    for (JsonPairConst section : params)
    {
        JsonObjectConst fields = section.value().as<JsonObjectConst>();
        for (JsonPairConst field : fields)
            _data[section.key()][field.key()] = field.value().as<float>();
    }

    Serial.println("[Config] Конфигурация загружена.");
    return true;
}

bool ConfigManager::saveToFile()
{
    if (!ensureMounted()) return false;

    File f = LittleFS.open("/config.json", "w");
    if (!f)
    {
        Serial.println("[Config] ОШИБКА: не удалось открыть файл для записи.");
        return false;
    }

    JsonDocument doc;
    doc["version"] = defaultsHash();
    doc["params"]  = _data;

    if (serializeJson(doc, f) == 0)
    {
        Serial.println("[Config] ОШИБКА: не удалось записать JSON.");
        f.close();
        return false;
    }

    f.close();
    Serial.println("[Config] Конфигурация сохранена.");
    return true;
}

bool ConfigManager::resetToDefaults()
{
    _applyDefaults();
    Serial.println("[Config] Сброс к значениям по умолчанию.");
    return saveToFile();
}

String ConfigManager::toJson() const
{
    String out;
    serializeJson(_data, out);
    return out;
}

bool ConfigManager::fromJson(const String &json)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
    {
        Serial.printf("[Config] ОШИБКА парсинга входящего JSON: %s\r\n", err.c_str());
        return false;
    }

    // Обновляем только те поля, которые пришли; остальные остаются как есть
    JsonObjectConst root = doc.as<JsonObjectConst>();
    for (JsonPairConst section : root)
    {
        JsonObjectConst fields = section.value().as<JsonObjectConst>();
        for (JsonPairConst field : fields)
            _data[section.key()][field.key()] = field.value().as<float>();
    }

    return saveToFile();
}
