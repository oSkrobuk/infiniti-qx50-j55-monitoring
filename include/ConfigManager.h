#pragma once
#include <Arduino.h>

// Структура для порогов температур одного датчика
struct TempConfig
{
    float min;
    float target;
    float max;
};

// Заводские значения по умолчанию — единственное место в проекте
namespace Defaults
{
    constexpr TempConfig OIL          = {50.0f, 90.0f,  98.0f};
    constexpr TempConfig COOLANT      = {50.0f, 90.0f, 100.0f};
    constexpr TempConfig RADIATOR     = { 0.0f, 50.0f,  90.0f};
    constexpr TempConfig TRANSMISSION = {50.0f, 80.0f, 100.0f};
}

class ConfigManager
{
public:
    TempConfig oil;
    TempConfig coolant;
    TempConfig radiator;
    TempConfig transmission;

    ConfigManager();

    // Монтирует LittleFS и загружает конфиг (вызывать в setup())
    bool init();

    // Загрузить конфиг из /config.json
    bool loadFromFile();

    // Сохранить конфиг в /config.json
    bool saveToFile();

    // Сбросить к заводским значениям и сохранить
    bool resetToDefaults();

    // Сериализовать текущий конфиг в JSON-строку
    String toJson() const;

    // Применить конфиг из JSON-строки, сохранить и вернуть true если успешно
    bool fromJson(const String &json);
};

// Глобальный объект конфига, доступен из всех файлов
extern ConfigManager config;
