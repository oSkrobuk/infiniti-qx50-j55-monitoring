#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager
{
private:
    TFT_eSPI tft;

public:
    DisplayManager();

    void init();

    uint16_t getTemperatureColor(float value, float minTemp, float targetTemp, float maxTemp);

    void updateMetrics(float coolant, float oil, float coolantR, float transmission);

    TFT_eSPI &getTft() { return tft; }
};
