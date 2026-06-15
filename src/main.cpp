#include <Arduino.h>
#include "DisplayManager.h"
#include "ConfigManager.h"
#include "WebManager.h"
#include "CanBusManager.h"

// Имя и пароль точки доступа ESP32
// Подключитесь к этой сети, затем откройте http://192.168.4.1
static const char *AP_SSID     = "QX50Monitoring";
static const char *AP_PASSWORD = "infiniti";

DisplayManager display;
WebManager     web(AP_SSID, AP_PASSWORD);

void setup()
{
    Serial.begin(115200);
    Serial.println("=== Infiniti QX50 J55 Monitoring ===");

    config.init();  // логирование внутри ConfigManager

    display.init();
    web.begin();

    // Инициализация CAN-шины (SN65HVD230 / WVCMCU-230)
    canBus.onFrame(canPrintFrame);
    canBus.init();

    Serial.printf("[Web] Адрес веб-интерфейса: http://%s\r\n", web.getIP().c_str());
    Serial.println("=====================================");
}

void loop()
{
    // Обрабатываем HTTP запросы
    web.handle();

    // Читаем фреймы с CAN-шины автомобиля (без delay — чтобы не терять фреймы)
    canBus.handle();

    // Обновляем дисплей не чаще 10 раз в секунду, без блокирующего delay
    static uint32_t lastDisplay = 0;
    if (millis() - lastDisplay >= 100)
    {
        lastDisplay = millis();

        // Имитация данных с датчиков автомобиля Infiniti для проверки отображения
        float mockCoolant      = 85.0f + sinf(millis() / 10000.0f) * 20.0f;
        float mockOil          = 90.0f + sinf(millis() / 10000.0f) * 20.0f;
        float mockCoolantR     = 50.0f + sinf(millis() / 10000.0f) * 70.0f;
        float mockTransmission = 80.0f + sinf(millis() / 10000.0f) * 40.0f;

        // Обновляем параметры на дисплее (работает плавно, без единого моргания)
        display.updateMetrics(mockCoolant, mockOil, mockCoolantR, mockTransmission);
    }
}
