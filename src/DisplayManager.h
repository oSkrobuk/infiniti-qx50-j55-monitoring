#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft;

public:
    DisplayManager();
    
    // Первоначальная отрисовка меню и рамок
    void init();
    
    // Обновление конкретных данных на экране (без мерцания)
    void updateMetrics(float coolant, float oil, float voltage, float boost);
    
    TFT_eSPI& getTft() { return tft; }
};
