#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Выделяем память под глобальный объект настроек
ConfigManager config;

// Конструктор: жестко прописываем заводские значения по умолчанию
ConfigManager::ConfigManager()
{
    oil = {50.0f, 90.0f, 110.0f};
    coolant = {50.0f, 90.0f, 105.0f};
    radiator = {-50.0f, 20.0f, 90.0f};
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
        saveToFile(); // Если файла нет, сохраняем то, что пришло из конструктора
        return true;
    }

    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile)
        return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error)
    {
        Serial.println("Ошибка парсинга JSON, используем дефолтные значения из конструктора");
        return false;
    }

    // Великолепное решение: если в JSON нет поля, переменная сохраняет свое значение из конструктора!
    oil.min = doc["oil"]["min"] | oil.min;
    oil.target = doc["oil"]["target"] | oil.target;
    oil.max = doc["oil"]["max"] | oil.max;

    coolant.min = doc["coolant"]["min"] | coolant.min;
    coolant.target = doc["coolant"]["target"] | coolant.target;
    coolant.max = doc["coolant"]["max"] | coolant.max;

    radiator.min = doc["radiator"]["min"] | radiator.min;
    radiator.target = doc["radiator"]["target"] | radiator.target;
    radiator.max = doc["radiator"]["max"] | radiator.max;

    return true;
}

bool ConfigManager::saveToFile()
{
    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile)
        return false;

    JsonDocument doc;
    doc["oil"]["min"] = oil.min;
    doc["oil"]["target"] = oil.target;
    doc["oil"]["max"] = oil.max;

    doc["coolant"]["min"] = coolant.min;
    doc["coolant"]["target"] = coolant.target;
    doc["coolant"]["max"] = coolant.max;

    doc["radiator"]["min"] = radiator.min;
    doc["radiator"]["target"] = radiator.target;
    doc["radiator"]["max"] = radiator.max;

    if (serializeJson(doc, configFile) == 0)
    {
        Serial.println("Ошибка записи JSON в файл!");
        configFile.close();
        return false;
    }

    configFile.close();
    return true;
}
