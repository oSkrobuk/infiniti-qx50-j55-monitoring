#pragma once
#include <Arduino.h>

// Структура для порогов температур
struct TempConfig
{
    float min;
    float target;
    float max;
};

class ConfigManager
{
public:
    // Переменные конфигурации
    TempConfig oil;
    TempConfig coolant;
    TempConfig radiator;

    // Конструктор по умолчанию (задаст базовые значения)
    ConfigManager();

    // Методы управления файлами
    bool init();
    bool loadFromFile();
    bool saveToFile();
};

// Делаем объект доступным для других файлов проекта (например, для DisplayManager)
extern ConfigManager config;
