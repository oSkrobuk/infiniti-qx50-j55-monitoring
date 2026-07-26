#pragma once

#include <stdint.h>

// Расчет цвета показаний в формате RGB565.
//
// Вынесено из DisplayManager намеренно: тот тянет <TFT_eSPI.h> и потому не
// собирается под хостом. Здесь только арифметика (HSV → RGB565) и чтение порогов
// из ConfigManager, поэтому цветовые зоны покрываются юнит-тестами.
// DisplayManager вызывает эти функции из одноименных методов
//
// Возвращаемые константы: 0xF800 — красный, 0x07E0 — зеленый,
// 0x001F — синий, 0xFFFF — белый (некорректные пороги)

// Цвет температуры: синий(≤min) → зеленый(target) → красный(≥max)
// Если пороги заданы некорректно (target вне диапазона min..max) — белый
uint16_t metric_temperature_color(float value, float min_temp, float target_temp, float max_temp);

// Цвет оборотов: синий(≤750) → зеленый(green_start..green_end) → красный(≥red_start)
uint16_t metric_rpm_color(float rpm);

// Цвет наддува: синий(≤blue_max) → желтый → зеленый(≥green_min)
uint16_t metric_boost_color(float boost);

// Цвет периода опроса RPM: синий(rpm=0) → зеленый(≤green_max) → красный(≥red_min)
uint16_t metric_poll_time_color(float poll_time, float rpm);

// Цвет вольтажа: красный(<red_low или >red_high) → желтый → зеленый(green_min..green_max)
uint16_t metric_battery_color(float voltage);

// Цвет давления масла: красный если ниже минимума для текущих оборотов
uint16_t metric_oil_pressure_color(float pressure, float rpm);
