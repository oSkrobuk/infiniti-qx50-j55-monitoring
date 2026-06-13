#include <Arduino.h>
#include "DisplayManager.h"

DisplayManager display;

void setup() {
    Serial.begin(115200);
    display.init();
}

void loop() {
    // Имитация данных с датчиков автомобиля Infiniti для проверки отображения
    float mockCoolant = 88.0 + sin(millis() / 5000.0) * 5.0; // Колебания 83-93 °C
    float mockOil     = 95.0 + sin(millis() / 8000.0) * 3.0; // Колебания 92-98 °C
    float mockVoltage = 14.1 + sin(millis() / 3000.0) * 0.2; // Колебания 13.9-14.3 V
    float mockBoost   = 0.4  + sin(millis() / 1500.0) * 0.6; // Надув от -0.2 до 1.0 bar

    // Обновляем параметры на дисплее (работает плавно, без единого моргания)
    display.updateMetrics(mockCoolant, mockOil, mockVoltage, mockBoost);
    
    delay(100); // Частота обновления экрана 10 раз в секунду
}
