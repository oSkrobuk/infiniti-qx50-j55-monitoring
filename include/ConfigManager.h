#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class ConfigManager
{
public:
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

    // Получить числовое значение поля для любого датчика/секции.
    // config.get("oil", "min"), config.get("rpm", "max"), etc.
    // Если ключ или поле не найдены — возвращает 0.0f.
    float get(const char *section, const char *field) const;

private:
    // Все текущие значения конфига — произвольная вложенная структура
    JsonDocument _data;

    // Применить значения по умолчанию в _data (не перезаписывает существующие)
    void _applyDefaults();
};

// Глобальный объект конфига, доступен из всех файлов
extern ConfigManager config;
