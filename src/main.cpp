#include <Arduino.h>
#include "DisplayManager.h"
#include "ConfigManager.h"
#include "WebManager.h"

// Имя и пароль точки доступа ESP32
// Подключитесь к этой сети, затем откройте http://192.168.4.1
static const char *AP_SSID     = "QX50Monitoring";
static const char *AP_PASSWORD = "infiniti";

DisplayManager display;
WebManager     web(AP_SSID, AP_PASSWORD);

void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== Infiniti QX50 J55 Monitoring ===");

    config.init();  // логирование внутри ConfigManager

    display.init();
    web.begin();

    Serial.printf("[Web] Адрес веб-интерфейса: http://%s\n", web.getIP().c_str());
    Serial.println("=====================================\n");
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
