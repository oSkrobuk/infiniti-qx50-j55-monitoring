#include <Arduino.h>
#include <math.h>

#include "CanBusManager.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "WebManager.h"

// Вернуть значение из CAN если оно свежее (обновлено не позднее stale_ms мс из конфига),
// иначе вернуть 0. Таймстамп 0 означает «ни разу не получено» — тоже 0
static float can_value(float value, uint32_t ts)
{
    if (ts == 0) return 0.0f;
    uint32_t stale_ms = static_cast<uint32_t>(config.get("system", "stale_ms"));
    return (millis() - ts <= stale_ms) ? value : 0.0f;
}

// Версия прошивки — отображается внизу дисплея
static constexpr const char *s_app_version = "2026.1.1";

// Имя и пароль точки доступа ESP32
// Подключитесь к этой сети, затем откройте http://192.168.4.1
static constexpr const char *s_ap_ssid     = "QX50Monitoring";
static constexpr const char *s_ap_password = "infiniti";

DisplayManager display;
WebManager     web(s_ap_ssid, s_ap_password);

// =============================================================================
// Планировщик UDS-опроса
// =============================================================================

// Адреса блоков управления
static constexpr uint32_t ECM_ID = 0x7E0; // Блок управления двигателем
static constexpr uint32_t TCM_ID = 0x7E1; // Блок управления трансмиссией

// Команда открытия расширенной диагностической сессии (0x10 0xC0)
static const uint8_t CMD_SESSION_OPEN[8] = { 0x02, 0x10, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Команда поддержания диагностической сессии (Tester Present, 0x3E 0x80 — без ответа)
static const uint8_t CMD_TESTER_PRESENT[8] = { 0x02, 0x3E, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Описание одного параметра для циклического опроса
struct PollEntry {
    uint32_t ecu_id; // Адрес блока: ECM_ID или TCM_ID
    uint16_t did;    // Data Identifier (UDS Service 0x22)
};

// Список параметров для опроса (строго по заданному порядку)
static constexpr PollEntry POLL_LIST[] = {
    { ECM_ID, 0x1201 }, // Обороты двигателя
    { ECM_ID, 0x110E }, // Давление наддува (турбина)
    { ECM_ID, 0x1278 }, // Давление масла ДВС
    { ECM_ID, 0x1103 }, // Напряжение бортовой сети
    { ECM_ID, 0x1101 }, // Температура ОЖ ДВС
    { ECM_ID, 0x111F }, // Температура масла ДВС
    { ECM_ID, 0x116B }, // Температура ОЖ радиатора
    { TCM_ID, 0x110C }, // Температура масла вариатора
};
static constexpr uint8_t POLL_COUNT = sizeof(POLL_LIST) / sizeof(POLL_LIST[0]);

// Интервал отправки Tester Present (мс)
static constexpr uint32_t TESTER_PRESENT_INTERVAL_MS = 2000;

// Текущий индекс в списке опроса
static uint8_t  s_poll_idx       = 0;

// Таймер между отправками запросов параметров
static uint32_t s_last_poll_ms   = 0;

// Таймер отправки Tester Present
static uint32_t s_last_tp_ms     = 0;

// Сформировать и отправить UDS ReadDataByIdentifier (Service 0x22) для заданного DID
static void poll_send_request(uint32_t ecu_id, uint16_t did)
{
    // Формат: [0x03][0x22][DID_HI][DID_LO][00 00 00 00]
    uint8_t req[8] = {
        0x03,
        0x22,
        static_cast<uint8_t>(did >> 8),
        static_cast<uint8_t>(did & 0xFF),
        0x00, 0x00, 0x00, 0x00
    };
    can_bus.send_frame(ecu_id, req, 8);
}

// Открыть расширенную диагностическую сессию на обоих блоках (вызывается в setup)
static void poll_open_session()
{
    can_bus.send_frame(ECM_ID, CMD_SESSION_OPEN, 8);
    delay(30); // небольшая пауза между двумя кадрами при инициализации
    can_bus.send_frame(TCM_ID, CMD_SESSION_OPEN, 8);
    Serial.println("[Poll] Расширенная сессия открыта (ECM + TCM)");
}

// Отправить Tester Present на оба блока (неблокирующий вызов из loop)
static void poll_tester_present()
{
    can_bus.send_frame(ECM_ID, CMD_TESTER_PRESENT, 8);
    can_bus.send_frame(TCM_ID, CMD_TESTER_PRESENT, 8);
}

// Вызывается в loop() — обрабатывает один шаг планировщика
static void poll_handle()
{
    uint32_t now = millis();

    // Tester Present каждые 2 секунды
    if (now - s_last_tp_ms >= TESTER_PRESENT_INTERVAL_MS) {
        s_last_tp_ms = now;
        poll_tester_present();
    }

    // Циклический опрос параметров с паузой из конфига (system.poll_interval_ms)
    uint32_t poll_interval_ms = static_cast<uint32_t>(config.get("system", "poll_interval_ms"));
    if (now - s_last_poll_ms >= poll_interval_ms) {
        s_last_poll_ms = now;

        const PollEntry &entry = POLL_LIST[s_poll_idx];
        poll_send_request(entry.ecu_id, entry.did);

        // Запоминаем момент отправки запроса оборотов для вычисления poll_time
        if (entry.ecu_id == ECM_ID && entry.did == 0x1201) {
            can_metrics.rpm_request_ts = now;
        }

        s_poll_idx = (s_poll_idx + 1) % POLL_COUNT;
    }
}

// =============================================================================

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

#ifndef USE_MOCK_DATA
    // Открываем расширенную диагностическую сессию на ECM и TCM
    poll_open_session();

    // Инициализируем таймеры планировщика
    s_last_poll_ms = millis();
    s_last_tp_ms   = millis();
#endif

    Serial.printf("[Web] Адрес веб-интерфейса: http://%s\r\n", web.get_ip().c_str());
    Serial.println("=====================================");
}

void loop()
{
    // Обрабатываем HTTP запросы
    web.handle();

    // Читаем фреймы с CAN-шины автомобиля (без delay — чтобы не терять фреймы)
    can_bus.handle();

#ifndef USE_MOCK_DATA
    // Планировщик UDS-опроса: Tester Present + циклический запрос параметров
    poll_handle();
#endif

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

        // Время опроса RPM: 0.01..0.60 с — имитация на основе синуса
        float mock_poll_time = 0.01f + (sinf(t * 0.5f) * 0.5f + 0.5f) * (0.60f - 0.01f);

        // Вольтаж бортовой сети: 11.0..15.0 Вольт
        // sinf дает -1..+1 → центр 13.0V, амплитуда 2.0V → диапазон 11.0..15.0
        float mock_battery_voltage = 13.0f + sinf(t * 0.3f) * 2.0f;

#ifdef USE_MOCK_DATA
        // --- МОКИ: имитация всех датчиков для отладки отображения ---
        float   coolant          = 85.0f + sinf(t) * 20.0f;
        float   oil              = 90.0f + sinf(t) * 20.0f;
        float   coolant_r        = 50.0f + sinf(t) * 70.0f;
        float   rpm              = mock_rpm;
        float   oil_pressure     = mock_oil_pressure;
        float   boost            = mock_boost;
        float   poll_time        = mock_poll_time;
        float   battery_voltage  = mock_battery_voltage;
        float   transmission     = mock_transmission;
#else
        // --- РЕАЛЬНЫЕ ДАННЫЕ: все метрики из CAN-шины (таймаут 500 мс → 0) ---
        float   coolant          = can_value(can_metrics.engine_coolant,    can_metrics.engine_coolant_ts);
        float   oil              = can_value(can_metrics.engine_oil,        can_metrics.engine_oil_ts);
        float   coolant_r        = can_value(can_metrics.radiator_coolant,  can_metrics.radiator_coolant_ts);
        float   rpm              = can_value(can_metrics.engine_rpm,        can_metrics.engine_rpm_ts);
        float   oil_pressure     = can_value(can_metrics.oil_pressure_volt, can_metrics.oil_pressure_volt_ts);
        float   boost            = can_value(can_metrics.turbo_boost_volt,  can_metrics.turbo_boost_volt_ts);
        float   poll_time        = can_value(can_metrics.rpm_poll_time,      can_metrics.rpm_poll_time_ts);
        float   battery_voltage  = can_value(can_metrics.battery_voltage,   can_metrics.battery_voltage_ts);
        float   transmission     = can_value(can_metrics.cvt_temp,          can_metrics.cvt_temp_ts);
#endif

        // Обновляем параметры на дисплее (работает плавно, без единого моргания)
        display.update_metrics(coolant, oil, coolant_r,
                               transmission, rpm,
                               oil_pressure, boost,
                               poll_time, battery_voltage);
    }
}
