#include <Arduino.h>
#include "DisplayManager.h"
#include "ConfigManager.h"

DisplayManager display;

void setup()
{
    Serial.begin(115200);

    if (config.init())
    {
        Serial.println("Конфигурация успешно загружена!");
    }

    display.init();
}

void loop()
{
    // Имитация данных с датчиков автомобиля Infiniti для проверки отображения
    float mockCoolant = 85.0 + sin(millis() / 10000.0) * 20.0;
    float mockOil = 90.0 + sin(millis() / 10000.0) * 20.0;
    float mockCoolantR = 50.0 + sin(millis() / 10000.0) * 70.0;
    float mockTransmission = 80 + sin(millis() / 10000.0) * 40.0;

    // Обновляем параметры на дисплее (работает плавно, без единого моргания)
    display.updateMetrics(mockCoolant, mockOil, mockCoolantR, mockTransmission);

    delay(100); // Частота обновления экрана 10 раз в секунду
}
