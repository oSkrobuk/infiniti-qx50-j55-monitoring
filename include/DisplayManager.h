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

    // Цвет вольтажа: красный(<red_low или >red_high) → жёлтый → зелёный(green_min..green_max)
    uint16_t get_battery_color(float voltage);

    // Цвет времени опроса RPM: синий(rpm=0) → зелёный(≤0.2с) → красный(≥0.5с)
    uint16_t get_poll_time_color(float poll_time, float rpm);

    void update_metrics(float coolant, float oil, float coolant_r,
                        float transmission, float rpm,
                        float oil_pressure, float boost,
                        float poll_time, float battery_voltage);

private:
    TFT_eSPI tft_;
};
