#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
public:
    DisplayManager();

    void init(const char *version = nullptr);

    uint16_t get_temperature_color(float value, float min_temp,
                                   float target_temp, float max_temp);

    // Цвет оборотов: синий(<green_start) → зелёный → жёлтый → красный(≥red_start)
    uint16_t get_rpm_color(float rpm);

    // Цвет давления масла: красный если ниже минимума для текущих оборотов
    uint16_t get_oil_pressure_color(float pressure, float rpm);

    // Цвет наддува: синий(≤blue_max) → жёлтый → зелёный(≥green_min)
    uint16_t get_boost_color(float boost);

    void update_metrics(float coolant, float oil, float coolant_r,
                        float transmission, float rpm,
                        float oil_pressure, float boost,
                        int8_t gear, float battery_voltage);

private:
    TFT_eSPI tft_;
};
