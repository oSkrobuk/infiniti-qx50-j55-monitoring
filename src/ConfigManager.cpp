#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

ConfigManager config;

static bool _fsMounted = false;

// ── Вспомогательные функции ──────────────────

static void writeSection(JsonDocument &doc, const char *key, const TempConfig &cfg)
{
    doc[key]["min"]    = cfg.min;
    doc[key]["target"] = cfg.target;
    doc[key]["max"]    = cfg.max;
}

static TempConfig readSection(const JsonDocument &doc, const char *key, const TempConfig &fallback)
{
    return {
        doc[key]["min"]    | fallback.min,
        doc[key]["target"] | fallback.target,
        doc[key]["max"]    | fallback.max,
    };
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

    oil          = readSection(doc, "oil",          oil);
    coolant      = readSection(doc, "coolant",      coolant);
    radiator     = readSection(doc, "radiator",     radiator);
    transmission = readSection(doc, "transmission", transmission);

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
    writeSection(doc, "oil",          oil);
    writeSection(doc, "coolant",      coolant);
    writeSection(doc, "radiator",     radiator);
    writeSection(doc, "transmission", transmission);

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
