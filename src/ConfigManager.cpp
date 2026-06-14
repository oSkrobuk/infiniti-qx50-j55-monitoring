#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Глобальный объект конфига
ConfigManager config;

// Конструктор: заводские значения по умолчанию
ConfigManager::ConfigManager()
{
    oil          = {50.0f, 90.0f,  98.0f};
    coolant      = {50.0f, 90.0f, 100.0f};
    radiator     = { 0.0f, 50.0f,  90.0f};
    transmission = {50.0f, 80.0f, 100.0f};
}

bool ConfigManager::init()
{
    if (!LittleFS.begin(true))
    {
        Serial.println("Ошибка монтирования LittleFS!");
        return false;
    }
    return loadFromFile();
}

bool ConfigManager::loadFromFile()
{
    if (!LittleFS.exists("/config.json"))
    {
        Serial.println("config.json не найден, сохраняем значения по умолчанию.");
        return saveToFile();
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f)
        return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err)
    {
        Serial.printf("Ошибка парсинга JSON: %s, используем значения по умолчанию.\n", err.c_str());
        return false;
    }

    oil.min    = doc["oil"]["min"]    | oil.min;
    oil.target = doc["oil"]["target"] | oil.target;
    oil.max    = doc["oil"]["max"]    | oil.max;

    coolant.min    = doc["coolant"]["min"]    | coolant.min;
    coolant.target = doc["coolant"]["target"] | coolant.target;
    coolant.max    = doc["coolant"]["max"]    | coolant.max;

    radiator.min    = doc["radiator"]["min"]    | radiator.min;
    radiator.target = doc["radiator"]["target"] | radiator.target;
    radiator.max    = doc["radiator"]["max"]    | radiator.max;

    transmission.min    = doc["transmission"]["min"]    | transmission.min;
    transmission.target = doc["transmission"]["target"] | transmission.target;
    transmission.max    = doc["transmission"]["max"]    | transmission.max;

    Serial.println("Конфиг загружен из файла.");
    return true;
}

bool ConfigManager::saveToFile()
{
    File f = LittleFS.open("/config.json", "w");
    if (!f)
    {
        Serial.println("Ошибка открытия config.json для записи!");
        return false;
    }

    JsonDocument doc;
    doc["oil"]["min"]    = oil.min;
    doc["oil"]["target"] = oil.target;
    doc["oil"]["max"]    = oil.max;

    doc["coolant"]["min"]    = coolant.min;
    doc["coolant"]["target"] = coolant.target;
    doc["coolant"]["max"]    = coolant.max;

    doc["radiator"]["min"]    = radiator.min;
    doc["radiator"]["target"] = radiator.target;
    doc["radiator"]["max"]    = radiator.max;

    doc["transmission"]["min"]    = transmission.min;
    doc["transmission"]["target"] = transmission.target;
    doc["transmission"]["max"]    = transmission.max;

    if (serializeJson(doc, f) == 0)
    {
        Serial.println("Ошибка записи JSON в файл!");
        f.close();
        return false;
    }

    f.close();
    Serial.println("Конфиг сохранён в файл.");
    return true;
}

String ConfigManager::toJson() const
{
    JsonDocument doc;
    doc["oil"]["min"]    = oil.min;
    doc["oil"]["target"] = oil.target;
    doc["oil"]["max"]    = oil.max;

    doc["coolant"]["min"]    = coolant.min;
    doc["coolant"]["target"] = coolant.target;
    doc["coolant"]["max"]    = coolant.max;

    doc["radiator"]["min"]    = radiator.min;
    doc["radiator"]["target"] = radiator.target;
    doc["radiator"]["max"]    = radiator.max;

    doc["transmission"]["min"]    = transmission.min;
    doc["transmission"]["target"] = transmission.target;
    doc["transmission"]["max"]    = transmission.max;

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
        Serial.printf("fromJson: ошибка парсинга: %s\n", err.c_str());
        return false;
    }

    // Применяем только те поля, которые пришли; остальные остаются прежними
    oil.min    = doc["oil"]["min"]    | oil.min;
    oil.target = doc["oil"]["target"] | oil.target;
    oil.max    = doc["oil"]["max"]    | oil.max;

    coolant.min    = doc["coolant"]["min"]    | coolant.min;
    coolant.target = doc["coolant"]["target"] | coolant.target;
    coolant.max    = doc["coolant"]["max"]    | coolant.max;

    radiator.min    = doc["radiator"]["min"]    | radiator.min;
    radiator.target = doc["radiator"]["target"] | radiator.target;
    radiator.max    = doc["radiator"]["max"]    | radiator.max;

    transmission.min    = doc["transmission"]["min"]    | transmission.min;
    transmission.target = doc["transmission"]["target"] | transmission.target;
    transmission.max    = doc["transmission"]["max"]    | transmission.max;

    return saveToFile();
}
