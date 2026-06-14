#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

ConfigManager config;

static bool _fsMounted = false;

// ── Вспомогательные функции ──────────────────

static void writeSection(JsonVariant doc, const char *key, const TempConfig &cfg)
{
    doc[key]["min"]    = cfg.min;
    doc[key]["target"] = cfg.target;
    doc[key]["max"]    = cfg.max;
}

static TempConfig readSection(JsonVariantConst doc, const char *key, const TempConfig &fallback)
{
    return {
        doc[key]["min"]    | fallback.min,
        doc[key]["target"] | fallback.target,
        doc[key]["max"]    | fallback.max,
    };
}

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

// Формирует JSON только из дефолтных значений и возвращает его CRC32
static uint32_t defaultsHash()
{
    JsonDocument doc;
    writeSection(doc, "oil",          Defaults::OIL);
    writeSection(doc, "coolant",      Defaults::COOLANT);
    writeSection(doc, "radiator",     Defaults::RADIATOR);
    writeSection(doc, "transmission", Defaults::TRANSMISSION);
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

// ── ConfigManager ────────────────────────────

ConfigManager::ConfigManager()
{
    oil          = Defaults::OIL;
    coolant      = Defaults::COOLANT;
    radiator     = Defaults::RADIATOR;
    transmission = Defaults::TRANSMISSION;
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
        Serial.printf("[Config] ОШИБКА парсинга JSON: %s\n", err.c_str());
        return false;
    }

    // Сравниваем хеш дефолтов из файла с текущим хешем дефолтов в коде.
    // Если значения по умолчанию изменились — перезаписываем файл.
    uint32_t fileHash    = doc["version"] | 0u;
    uint32_t currentHash = defaultsHash();

    if (fileHash != currentHash)
    {
        Serial.printf("[Config] Значения по умолчанию изменились (hash %08X → %08X), сбрасываем конфиг.\n",
                      fileHash, currentHash);
        return saveToFile();
    }

    oil          = readSection(doc["params"], "oil",          oil);
    coolant      = readSection(doc["params"], "coolant",      coolant);
    radiator     = readSection(doc["params"], "radiator",     radiator);
    transmission = readSection(doc["params"], "transmission", transmission);

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

    // Формируем params отдельно чтобы посчитать хеш от них
    JsonDocument params;
    writeSection(params, "oil",          oil);
    writeSection(params, "coolant",      coolant);
    writeSection(params, "radiator",     radiator);
    writeSection(params, "transmission", transmission);

    // Хеш считается от дефолтных значений (не от текущих params)
    // — это позволяет обнаружить изменение дефолтов при следующей прошивке
    JsonDocument doc;
    doc["version"] = defaultsHash();
    doc["params"]  = params;

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
    oil          = Defaults::OIL;
    coolant      = Defaults::COOLANT;
    radiator     = Defaults::RADIATOR;
    transmission = Defaults::TRANSMISSION;
    Serial.println("[Config] Сброс к значениям по умолчанию.");
    return saveToFile();
}

String ConfigManager::toJson() const
{
    JsonDocument doc;
    writeSection(doc, "oil",          oil);
    writeSection(doc, "coolant",      coolant);
    writeSection(doc, "radiator",     radiator);
    writeSection(doc, "transmission", transmission);

    String out;
    serializeJson(doc, out);
    return out;
}

bool ConfigManager::fromJson(const String &json)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
    {
        Serial.printf("[Config] ОШИБКА парсинга входящего JSON: %s\n", err.c_str());
        return false;
    }

    oil          = readSection(doc, "oil",          oil);
    coolant      = readSection(doc, "coolant",      coolant);
    radiator     = readSection(doc, "radiator",     radiator);
    transmission = readSection(doc, "transmission", transmission);

    return saveToFile();
}
