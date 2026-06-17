#include <Arduino.h>

#include "CanBusManager.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "WebManager.h"

// Имя и пароль точки доступа ESP32
// Подключитесь к этой сети, затем откройте http://192.168.4.1
static constexpr const char *s_ap_ssid     = "QX50Monitoring";
static constexpr const char *s_ap_password = "infiniti";

DisplayManager display;
WebManager     web(s_ap_ssid, s_ap_password);

void setup()
{
    Serial.begin(115200);
    Serial.println("=== Infiniti QX50 J55 Monitoring ===");

    config.init();

    display.init();
    web.begin();

    // Инициализация CAN-шины (SN65HVD230 / WVCMCU-230)
    can_bus.on_frame(can_print_frame);
    can_bus.init();

    Serial.printf("[Web] Адрес веб-интерфейса: http://%s\r\n", web.get_ip().c_str());
    Serial.println("=====================================");
}

void loop()
{
    // Обрабатываем HTTP запросы
    web.handle();

    // Читаем фреймы с CAN-шины автомобиля (без delay — чтобы не терять фреймы)
    can_bus.handle();

    // Обновляем дисплей не чаще 10 раз в секунду, без блокирующего delay
    static uint32_t s_last_display = 0;
    if (millis() - s_last_display >= 100) {
        s_last_display = millis();

        // Имитация данных с датчиков автомобиля Infiniti для проверки отображения
        float t = millis() / 3000.0f;

        float mock_coolant      = 85.0f + sinf(t)        * 20.0f;
        float mock_oil          = 90.0f + sinf(t)        * 20.0f;
        float mock_coolant_r    = 50.0f + sinf(t)        * 70.0f;
        float mock_transmission = 80.0f + sinf(t)        * 40.0f;

        // Обороты двигателя: 750..6000 об/мин
        float mock_rpm = 750.0f +
            (sinf(t * 0.7f) * 0.5f + 0.5f) * (6000.0f - 750.0f);

        // Давление масла: 1.3..3.5 бар
        float mock_oil_pressure =
            1.3f + (sinf(t * 1.3f) * 0.5f + 0.5f) * (3.5f - 1.3f);

        // Давление наддува: -0.50..1.20 бар
        // sinf даёт -1..+1 → центр 0.35, амплитуда 0.85 → диапазон -0.5..1.2
        float mock_boost = sinf(t * 0.9f) * 0.85f + 0.35f;

        // Обновляем параметры на дисплее (работает плавно, без единого моргания)
        display.update_metrics(mock_coolant, mock_oil, mock_coolant_r,
                               mock_transmission, mock_rpm,
                               mock_oil_pressure, mock_boost);
    }
}
