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
    float mockCoolant = 88.0 + sin(millis() / 5000.0) * 5.0;
    float mockOil = 95.0 + sin(millis() / 8000.0) * 3.0;
    float mockCoolantR = 88.0 + sin(millis() / 3000.0) * 5.0;

    // Обновляем параметры на дисплее (работает плавно, без единого моргания)
    display.updateMetrics(mockCoolant, mockOil, mockCoolantR);

    delay(100); // Частота обновления экрана 10 раз в секунду
}
