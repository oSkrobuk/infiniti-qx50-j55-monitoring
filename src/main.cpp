#include <Arduino.h>

#include "CanBusManager.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "WebManager.h"

// Вернуть значение из CAN если оно свежее (обновлено не позднее CAN_STALE_MS мс),
// иначе вернуть 0. Таймстамп 0 означает «ни разу не получено» — тоже 0
static float can_value(float value, uint32_t ts)
{
    if (ts == 0) return 0.0f;
    return (millis() - ts <= CAN_STALE_MS) ? value : 0.0f;
}

// Версия прошивки — отображается внизу дисплея
static constexpr const char *s_app_version = "2026.1.1";

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

    display.init(s_app_version);
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

        float t = millis() / 3000.0f;

        // Имитация данных с датчиков автомобиля Infiniti для проверки отображения
        float mock_transmission = 80.0f + sinf(t) * 40.0f;

        // Обороты двигателя: 750..6000 об/мин
        float mock_rpm = 750.0f +
            (sinf(t * 0.7f) * 0.5f + 0.5f) * (6000.0f - 750.0f);

        // Давление масла: 1.3..3.5 бар
        float mock_oil_pressure =
            1.3f + (sinf(t * 1.3f) * 0.5f + 0.5f) * (3.5f - 1.3f);

        // Давление наддува (Вольты): 0.50..4.50 В
        // sinf даёт -1..+1 → центр 2.50, амплитуда 2.00 → диапазон 0.5..4.5
        float mock_boost = 2.50f + sinf(t * 0.9f) * 2.00f;

        // Номер передачи: от 1 до 8 (округляем синусоиду до целого числа)
        // sinf дает -1..+1 → приводим к 0..1, умножаем на разницу (7) и прибавляем минимум (1)
        int8_t mock_gear = static_cast<int8_t>(roundf(1.0f + (sinf(t * 0.4f) * 0.5f + 0.5f) * 7.0f));

        // Вольтаж бортовой сети: 11.0..15.0 Вольт
        // sinf дает -1..+1 → центр 13.0V, амплитуда 2.0V → диапазон 11.0..15.0
        float mock_battery_voltage = 13.0f + sinf(t * 0.3f) * 2.0f;

#ifdef USE_MOCK_DATA
        // --- МОКИ: имитация температур для отладки отображения ---
        float coolant   = 85.0f + sinf(t) * 20.0f;
        float oil       = 90.0f + sinf(t) * 20.0f;
        float coolant_r = 50.0f + sinf(t) * 70.0f;
#else
        // --- РЕАЛЬНЫЕ ДАННЫЕ: температуры из CAN-шины (таймаут 500 мс → 0) ---
        float coolant   = can_value(can_metrics.engine_coolant,   can_metrics.engine_coolant_ts);
        float oil       = can_value(can_metrics.engine_oil,       can_metrics.engine_oil_ts);
        float coolant_r = can_value(can_metrics.radiator_coolant, can_metrics.radiator_coolant_ts);
#endif

        // Обновляем параметры на дисплее (работает плавно, без единого моргания)
        display.update_metrics(coolant, oil, coolant_r,
                               mock_transmission, mock_rpm,
                               mock_oil_pressure, mock_boost,
                               mock_gear, mock_battery_voltage);
    }
}
