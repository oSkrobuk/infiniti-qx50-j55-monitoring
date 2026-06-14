#pragma once
#include <Arduino.h>

// Структура для порогов температур одного датчика
struct TempConfig
{
    float min;
    float target;
    float max;
};

class ConfigManager
{
public:
    TempConfig oil;
    TempConfig coolant;
    TempConfig radiator;
    TempConfig transmission;

    ConfigManager();

    // Монтирует LittleFS и загружает конфиг
    bool init();

    // Загрузить конфиг из /config.json
    bool loadFromFile();

    // Сохранить конфиг в /config.json
    bool saveToFile();

    // Сериализовать текущий конфиг в JSON-строку (для HTTP ответа)
    String toJson() const;

    // Применить конфиг из JSON-строки (из HTTP запроса), вернуть true если успешно
    bool fromJson(const String &json);
};

// Глобальный объект конфига, доступен из всех файлов
extern ConfigManager config;
