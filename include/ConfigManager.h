#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class ConfigManager {
public:
    ConfigManager();

    // Монтирует LittleFS и загружает конфиг (вызывать в setup())
    bool init();

    // Загрузить конфиг из /config.json
    bool load_from_file();

    // Сохранить конфиг в /config.json
    bool save_to_file();

    // Сбросить к заводским значениям и сохранить
    bool reset_to_defaults();

    // Сериализовать текущий конфиг в JSON-строку
    String to_json() const;

    // Применить конфиг из JSON-строки, сохранить и вернуть true если успешно
    bool from_json(const String &json);

    // Получить числовое значение поля для любого датчика/секции
    // config.get("oil", "min"), config.get("rpm", "max"), etc.
    // Если ключ или поле не найдены — возвращает 0.0f
    float get(const char *section, const char *field) const;

    // Получить строковое значение поля (например, WiFi ssid/password)
    // Если поле не найдено — возвращает пустую строку ""
    String get_str(const char *section, const char *field) const;

private:
    // Все текущие значения конфига — произвольная вложенная структура
    JsonDocument data_;

    // Применить значения по умолчанию в data_
    void apply_defaults();
};

// Глобальный объект конфига, доступен из всех файлов
extern ConfigManager config;
