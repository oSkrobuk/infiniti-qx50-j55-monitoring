#include <Arduino.h>
#include "DisplayManager.h"
#include "ConfigManager.h"
#include "WebManager.h"

// Имя и пароль точки доступа ESP32
// Подключитесь к этой сети, затем откройте http://192.168.4.1
static const char *AP_SSID     = "Infiniti-QX50";
static const char *AP_PASSWORD = "infiniti123";

DisplayManager display;
WebManager     web(AP_SSID, AP_PASSWORD);

void setup()
{
    Serial.begin(115200);

    if (!config.init())
    {
        Serial.println("ПРЕДУПРЕЖДЕНИЕ: конфигурация не загружена, используются значения по умолчанию.");
    }
    else
    {
        Serial.println("Конфигурация успешно загружена!");
    }

    display.init();
    web.begin();

    Serial.printf("Веб-интерфейс доступен по адресу: http://%s\n", web.getIP().c_str());
}

void loop()
{
    // Обрабатываем HTTP запросы
    web.handle();

    // Имитация данных с датчиков автомобиля Infiniti для проверки отображения
    float mockCoolant      = 85.0f + sinf(millis() / 10000.0f) * 20.0f;
    float mockOil          = 90.0f + sinf(millis() / 10000.0f) * 20.0f;
    float mockCoolantR     = 50.0f + sinf(millis() / 10000.0f) * 70.0f;
    float mockTransmission = 80.0f + sinf(millis() / 10000.0f) * 40.0f;

    // Обновляем параметры на дисплее (работает плавно, без единого моргания)
    display.updateMetrics(mockCoolant, mockOil, mockCoolantR, mockTransmission);

    delay(100); // Частота обновления экрана 10 раз в секунду
}
