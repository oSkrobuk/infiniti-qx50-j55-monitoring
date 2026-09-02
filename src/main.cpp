#include <Arduino.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "CanBusManager.h"
#include "ConfigManager.h"
#include "WebManager.h"
#include "BuzzerController.h"
#include "AlertManager.h"
#include "OtaImage.h"
#include "ObdPidCatalog.h"
#include "ResetHistory.h"
#include "Version.h"

// Выбор менеджера дисплея в зависимости от целевой платформы
#if defined(DISPLAY_WT32_S3)
#include "DisplayManagerWT32.h"
DisplayManagerWT32 display;
#else
#include "DisplayManager.h"
DisplayManager     display;
#endif

// Вернуть значение из CAN если оно свежее (обновлено не позднее stale_ms мс из конфига),
// иначе вернуть 0. Таймстамп 0 означает «ни разу не получено» — тоже 0
static float can_value(float value, uint32_t ts)
{
    if (ts == 0) return 0.0f;
    uint32_t stale_ms = static_cast<uint32_t>(config.get("system", "stale_ms"));
    return (millis() - ts <= stale_ms) ? value : 0.0f;
}

WebManager       web;
BuzzerController buzzer;

// =============================================================================
// Веб-сервер в отдельной задаче
// =============================================================================
//
// Конечный автомат WebServer делает один шаг за вызов handleClient(): принять
// сокет, дождаться запроса, разобрать его, ответить. Пока эти шаги висели в
// loop(), их темп задавала самая медленная часть цикла — отрисовка дисплея.
// В своей задаче на ядре 0 шаги идут раз в миллисекунду независимо от loop().
//
// Плата за это — общее состояние (config, alert_manager, can_metrics) теперь
// читается и пишется из двух задач. Мьютекс ниже разводит доступ: веб-задача
// держит его на время handleClient(), loop() — на время своей итерации.
// Без мьютекса POST /config, перестраивающий JsonDocument, мог бы совпасть
// с config.get() из loop() и уронить прошивку

// Стек веб-задачи: обработчикам нужны ArduinoJson, String и буферы OTA
static constexpr uint32_t WEB_TASK_STACK_BYTES = 8192;

// Ядро для веб-задачи: loopTask Arduino работает на ядре 1
static constexpr BaseType_t WEB_TASK_CORE = 0;

// Приоритет как у loopTask — веб не должен вытеснять основной цикл
static constexpr UBaseType_t WEB_TASK_PRIORITY = 1;

// Мьютекс общего состояния между loop() и веб-задачей
static SemaphoreHandle_t s_state_mutex = nullptr;

// true после успешного запуска веб-задачи. Если задачу или мьютекс создать
// не удалось, HTTP остается в loop() — интерфейс работает медленно, но работает
static bool s_web_in_task = false;

// Захват и освобождение общего состояния. Пока мьютекса нет (setup до запуска
// веб-задачи) вызовы безопасно превращаются в no-op
static inline void state_lock()
{
    if (s_state_mutex != nullptr) xSemaphoreTake(s_state_mutex, portMAX_DELAY);
}

static inline void state_unlock()
{
    if (s_state_mutex != nullptr) xSemaphoreGive(s_state_mutex);
}

static void web_task(void *)
{
    for (;;) {
        state_lock();
        web.handle();
        state_unlock();

        // Пауза обязательна: без нее задача с приоритетом loopTask не пускает
        // к работе idle-задачу ядра 0 и срабатывает Task WDT. Один тик FreeRTOS
        // (1 мс) на шаг автомата — на порядок быстрее прежней привязки к loop()
        vTaskDelay(1);
    }
}

// Запустить HTTP-сервер в отдельной задаче. Вызывать в конце setup(), когда
// конфиг, журнал алертов и CAN уже инициализированы: обработчики читают их
// сразу после первого же запроса
static void web_task_start()
{
    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == nullptr) {
        Serial.println("[Web] Мьютекс не создан — сервер остается в loop()");
        return;
    }

    if (xTaskCreatePinnedToCore(web_task, "web", WEB_TASK_STACK_BYTES, nullptr,
                                WEB_TASK_PRIORITY, nullptr, WEB_TASK_CORE) != pdPASS) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = nullptr;
        Serial.println("[Web] Задача не создана — сервер остается в loop()");
        return;
    }

    s_web_in_task = true;
    Serial.printf("[Web] Сервер вынесен в отдельную задачу на ядре %d\r\n",
                  static_cast<int>(WEB_TASK_CORE));
}

// =============================================================================
// Планировщик UDS-опроса
// =============================================================================

// Адреса блоков управления
static constexpr uint32_t ECM_ID = 0x7E0; // Блок управления двигателем
static constexpr uint32_t TCM_ID = 0x7E1; // Блок управления трансмиссией
static constexpr uint32_t LIGHT_MODULE_ID = 0x743; // Блок состояния освещения
static constexpr uint32_t LIGHT_RESPONSE_ID = 0x763; // Ответ блока состояния освещения
static constexpr uint16_t LIGHT_STATUS_DID = 0x0E07;

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
static constexpr uint32_t LIGHT_POLL_INTERVAL_MS = 5000;
static constexpr uint32_t OBD_POLL_INTERVAL_MS = 1000;
static constexpr uint32_t OBD_DISCOVERY_TIMEOUT_MS = 100;
static constexpr uint8_t OBD_DISCOVERY_RETRIES = 3;
static constexpr uint8_t OBD_POLL_LIST[] = {
    0x04, 0x05, 0x06, 0x07, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
    0x1F, 0x23, 0x24, 0x2F, 0x33, 0x3C, 0x43, 0x44, 0x46, 0x49, 0x4A, 0x5C,
    0x5E, 0x61, 0x62, 0x63, 0x64,
};
static constexpr uint8_t OBD_POLL_COUNT = sizeof(OBD_POLL_LIST);

// Текущий индекс в списке опроса
static uint8_t  s_poll_idx       = 0;

// Таймер между отправками запросов параметров
static uint32_t s_last_poll_ms   = 0;

// Таймер отправки Tester Present
static uint32_t s_last_tp_ms     = 0;

// Таймер проверки состояния освещения
static uint32_t s_last_light_poll_ms = 0;
static ObdPidCatalog s_obd_catalog;
static bool s_diagnostic_session_open = false;
static uint8_t s_obd_discovery_base = 0xFF;
static uint8_t s_obd_discovery_retries = 0;
static uint32_t s_obd_discovery_sent_ms = 0;
static uint8_t s_obd_poll_idx = OBD_POLL_COUNT;
static uint32_t s_obd_cycle_started_ms = 0;

// Принять ответы штатного мониторинга и завершить ISO-TP обмен с блоком света
static void can_handle_monitor_frame(const CanFrame &frame)
{
    can_print_frame(frame);
    s_obd_catalog.accept(frame);

    if (frame.id != LIGHT_RESPONSE_ID || frame.dlc < 7) {
        return;
    }

    const uint8_t *d = frame.data;
    const bool is_first_frame = (d[0] & 0xF0) == 0x10;
    const uint16_t payload_length = (static_cast<uint16_t>(d[0] & 0x0F) << 8) | d[1];
    const bool is_light_status = d[2] == 0x62 && d[3] == static_cast<uint8_t>(LIGHT_STATUS_DID >> 8) &&
                                 d[4] == static_cast<uint8_t>(LIGHT_STATUS_DID & 0xFF);
    if (!is_first_frame || payload_length < 5 || !is_light_status) {
        return;
    }

    // Разрешить блоку передать оставшиеся Consecutive Frame
    const uint8_t flow_control[8] = { 0x30, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    can_bus.send_frame(LIGHT_MODULE_ID, flow_control, 8);

    // Второй байт данных DID: 0x0C = OFF, 0x1C = ON, меняется бит 0x10
    Serial.printf("[Light] %s raw=0x%02X\r\n", can_metrics.exterior_light_on ? "ON" : "OFF", d[6]);
}

static bool obd_send_request(uint8_t pid)
{
    const uint8_t req[8] = {0x02, 0x01, pid, 0, 0, 0, 0, 0};
    return can_bus.try_send_frame(ECM_ID, req, 8);
}

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

// Открыть расширенную диагностическую сессию на опрашиваемых блоках
static void poll_open_session()
{
    can_bus.send_frame(ECM_ID, CMD_SESSION_OPEN, 8);
    delay(30); // Небольшая пауза между кадрами при инициализации
    can_bus.send_frame(TCM_ID, CMD_SESSION_OPEN, 8);
    delay(30);
    can_bus.send_frame(LIGHT_MODULE_ID, CMD_SESSION_OPEN, 8);
    Serial.println("[Poll] Расширенная сессия открыта (ECM + TCM + LIGHT)");
}

// Отправить Tester Present на опрашиваемые блоки
static void poll_tester_present()
{
    can_bus.send_frame(ECM_ID, CMD_TESTER_PRESENT, 8);
    can_bus.send_frame(TCM_ID, CMD_TESTER_PRESENT, 8);
    can_bus.send_frame(LIGHT_MODULE_ID, CMD_TESTER_PRESENT, 8);
}

// Вызывается в loop() — обрабатывает один шаг планировщика
static void poll_handle()
{
    uint32_t now = millis();

    if (!s_diagnostic_session_open) {
        const uint8_t next_base = s_obd_catalog.next_query_base();
        if (s_obd_catalog.complete() || s_obd_discovery_retries >= OBD_DISCOVERY_RETRIES) {
            poll_open_session();
            s_diagnostic_session_open = true;
            s_last_poll_ms = millis();
            return;
        }
        const bool new_range = next_base != s_obd_discovery_base;
        const bool timed_out = now - s_obd_discovery_sent_ms >= OBD_DISCOVERY_TIMEOUT_MS;
        if (new_range || timed_out) {
            if (new_range) s_obd_discovery_retries = 0;
            if (obd_send_request(next_base)) {
                s_obd_discovery_base = next_base;
                s_obd_discovery_retries++;
                s_obd_discovery_sent_ms = now;
            }
        }
        return;
    }

    // Tester Present каждые 2 секунды
    if (now - s_last_tp_ms >= TESTER_PRESENT_INTERVAL_MS) {
        s_last_tp_ms = now;
        poll_tester_present();
    }

    // Состояние освещения меняется редко, поэтому достаточно опроса раз в 5 секунд
    if (now - s_last_light_poll_ms >= LIGHT_POLL_INTERVAL_MS) {
        s_last_light_poll_ms = now;
        poll_send_request(LIGHT_MODULE_ID, LIGHT_STATUS_DID);
    }

    // Циклический опрос параметров с паузой из конфига (system.poll_interval_ms)
    uint32_t poll_interval_ms = static_cast<uint32_t>(config.get("system", "poll_interval_ms"));
    if (now - s_last_poll_ms >= poll_interval_ms) {
        s_last_poll_ms = now;

        const PollEntry &entry = POLL_LIST[s_poll_idx];
        poll_send_request(entry.ecu_id, entry.did);

        // Вычисляем период обновления RPM (время между двумя последовательными отправками запроса)
        // rpm_poll_time показывает реальный интервал обновления данных оборотов = poll_interval_ms * POLL_COUNT
        if (entry.ecu_id == ECM_ID && entry.did == 0x1201) {
            if (can_metrics.rpm_request_ts != 0) {
                uint32_t elapsed_ms       = now - can_metrics.rpm_request_ts;
                can_metrics.rpm_poll_time    = static_cast<float>(elapsed_ms) / 1000.0f;
                can_metrics.rpm_poll_time_ts = now;
            }
            can_metrics.rpm_request_ts = now;
        }

        s_poll_idx = (s_poll_idx + 1) % POLL_COUNT;
    }

    if (s_obd_poll_idx >= OBD_POLL_COUNT &&
        now - s_obd_cycle_started_ms >= OBD_POLL_INTERVAL_MS) {
        s_obd_cycle_started_ms = now;
        s_obd_poll_idx = 0;
    }

    while (s_obd_poll_idx < OBD_POLL_COUNT) {
        if (millis() - s_last_poll_ms >= poll_interval_ms) return;
        const uint8_t pid = OBD_POLL_LIST[s_obd_poll_idx];
        if (!s_obd_catalog.supports(pid)) {
            s_obd_poll_idx++;
            continue;
        }
        if (!obd_send_request(pid)) return;
        s_obd_poll_idx++;
    }
}

// =============================================================================

void setup()
{
#ifdef DID_HOOK_MODE
    Serial.begin(DID_HOOK_SERIAL_BAUD);
#else
    Serial.begin(115200);
#endif
#ifdef DID_HOOK_MODE
    Serial.println("=== Infiniti QX50 J55 DID Hook ===");

    config.init();
    reset_history.init();
    display.init(FW_VERSION);
    display.show_alert("HOOK", "DID\nHOOK MODE\nSERIAL LOG");
    web.begin();
    alert_manager.init();

    can_bus.on_frame(can_print_did_hook_frame);
    can_bus.init();
    web_task_start();

    Serial.printf("[HOOK] Пассивный перехват всех CAN ID, Serial %lu\r\n",
                  static_cast<unsigned long>(DID_HOOK_SERIAL_BAUD));
    Serial.printf("[HOOK] Firmware marker: %s\r\n", FW_IMAGE_TAG);
    Serial.printf("[Web] Адрес веб-интерфейса: http://%s\r\n", web.get_ip().c_str());
    Serial.println("=====================================");
    return;
#else
    Serial.println("=== Infiniti QX50 J55 Monitoring ===");

    config.init();
    reset_history.init();

    // Версия прошивки (FW_VERSION из include/Version.h) — отображается внизу дисплея
    display.init(FW_VERSION);
    web.begin();

    // Инициализация менеджера алертов (после монтирования LittleFS в config.init())
    alert_manager.init();

#ifndef USE_MOCK_DATA
    // Инициализация CAN-шины (SN65HVD230 / WVCMCU-230)
    can_bus.on_frame(can_handle_monitor_frame);
    can_bus.init();

    // Инициализируем таймеры планировщика
    s_last_poll_ms = millis();
    s_last_tp_ms   = millis();
    s_last_light_poll_ms = millis();
#endif

    // Все общее состояние готово — можно пускать HTTP-обработчики в свою задачу
    web_task_start();

    Serial.printf("[Web] Адрес веб-интерфейса: http://%s\r\n", web.get_ip().c_str());
    Serial.println("=====================================");

    // Звук загрузки, как у BIOS
    buzzer.hello();
#endif
}

void loop()
{
#ifdef DID_HOOK_MODE
    if (!s_web_in_task) {
        web.handle();
    }

    can_bus.handle();

    static uint32_t s_last_hook_status_ms = 0;
    const uint32_t now = millis();
    if (now - s_last_hook_status_ms >= 2000) {
        s_last_hook_status_ms = now;
        Serial.printf("[HOOK] CAN state=%s RX=%lu errors=%lu bus_off=%lu\r\n",
                      can_bus.state_name(),
                      static_cast<unsigned long>(can_bus.received_count()),
                      static_cast<unsigned long>(can_bus.error_count()),
                      static_cast<unsigned long>(can_bus.bus_off_count()));
    }

    delay(1);
#else
    // Общее состояние делим с веб-задачей — держим мьютекс всю итерацию.
    // Итерация короткая (десятки микросекунд, раз в 100 мс — отрисовка),
    // поэтому веб-задача почти никогда не ждет на этом мьютексе
    state_lock();

    // Ждем триггера для бузера
    buzzer.update();

    // Обрабатываем HTTP запросы, если веб-задачу поднять не удалось
    if (!s_web_in_task) {
        web.handle();
    }

    // Читаем фреймы с CAN-шины автомобиля (без delay — чтобы не терять фреймы)
    can_bus.handle();

#ifndef USE_MOCK_DATA
    // Планировщик UDS-опроса: Tester Present + циклический запрос параметров
    poll_handle();
#endif

    // Проверяем алерты раз в 200 мс
    static uint32_t s_last_alert_check = 0;
    if (millis() - s_last_alert_check >= 200) {
        s_last_alert_check = millis();
        alert_manager.update(can_metrics);
    }

    // Обновляем дисплей не чаще 10 раз в секунду, без блокирующего delay
    static uint32_t s_last_display = 0;
    if (millis() - s_last_display >= 100) {
        s_last_display = millis();

        float t = millis() / 3000.0f;

        // Имитация данных с датчиков автомобиля Infiniti для проверки отображения
        float mock_transmission = 80.0f + sinf(t * 2.0f) * 40.0f;

        // Обороты двигателя: 750..6000 об/мин
        float mock_rpm = 750.0f +
            (sinf(t * 2.1f) * 0.5f + 0.5f) * (6000.0f - 750.0f);

        // Напряжение датчика давления масла: 1.3..3.5 В
        float mock_oil_pressure =
            1.3f + (sinf(t * 2.3f) * 0.5f + 0.5f) * (3.5f - 1.3f);

        // Давление наддува (Вольты): 0.50..4.50 В
        // sinf даёт -1..+1 → центр 2.50, амплитуда 2.00 → диапазон 0.5..4.5
        float mock_boost = 2.50f + sinf(t * 1.9f) * 2.00f;

        // Период обновления RPM: 0.01..0.60 с — имитация на основе синуса
        float mock_poll_time = 0.01f + (sinf(t * 2.5f) * 0.5f + 0.5f) * (0.60f - 0.01f);

        // Вольтаж бортовой сети: 11.0..15.0 Вольт
        // sinf дает -1..+1 → центр 13.0V, амплитуда 2.0V → диапазон 11.0..15.0
        float mock_battery_voltage = 13.0f + sinf(t * 1.8f) * 2.0f;

#ifdef USE_MOCK_DATA
        // --- МОКИ: имитация всех датчиков для отладки отображения ---
        float   coolant          = 85.0f + sinf(t * 2.0f) * 20.0f;
        float   oil              = 90.0f + sinf(t * 2.2f) * 20.0f;
        float   coolant_r        = 50.0f + sinf(t * 1.7f) * 70.0f;
        float   rpm              = mock_rpm;
        float   oil_pressure     = mock_oil_pressure;
        float   boost            = mock_boost;
        float   poll_time        = mock_poll_time;
        float   battery_voltage  = mock_battery_voltage;
        float   transmission     = mock_transmission;

        // Записываем мок-данные в can_metrics — AlertManager читает именно оттуда
        {
            uint32_t ts = millis();
            can_metrics.engine_oil           = oil;
            can_metrics.engine_oil_ts        = ts;
            can_metrics.engine_coolant       = coolant;
            can_metrics.engine_coolant_ts    = ts;
            can_metrics.radiator_coolant     = coolant_r;
            can_metrics.radiator_coolant_ts  = ts;
            can_metrics.cvt_temp             = transmission;
            can_metrics.cvt_temp_ts          = ts;
            can_metrics.engine_rpm           = rpm;
            can_metrics.engine_rpm_ts        = ts;
            can_metrics.battery_voltage      = battery_voltage;
            can_metrics.battery_voltage_ts   = ts;
            can_metrics.oil_pressure_volt    = oil_pressure;
            can_metrics.oil_pressure_volt_ts = ts;
            can_metrics.turbo_boost_volt     = boost;
            can_metrics.turbo_boost_volt_ts  = ts;
        }
#else
        // --- РЕАЛЬНЫЕ ДАННЫЕ: все метрики из CAN-шины (таймаут из конфига) ---
        float   coolant          = can_value(can_metrics.engine_coolant,    can_metrics.engine_coolant_ts);
        float   oil              = can_value(can_metrics.engine_oil,        can_metrics.engine_oil_ts);
        float   coolant_r        = can_value(can_metrics.radiator_coolant,  can_metrics.radiator_coolant_ts);
        float   rpm              = can_value(can_metrics.engine_rpm,        can_metrics.engine_rpm_ts);
        float   oil_pressure     = can_value(can_metrics.oil_pressure_volt, can_metrics.oil_pressure_volt_ts);
        float   boost            = can_value(can_metrics.turbo_boost_volt,  can_metrics.turbo_boost_volt_ts);
        // rpm_poll_time — результат последнего замера, а не живой поток:
        // не ограничиваем stale_ms, иначе при большом poll_interval_ms (>stale_ms/POLL_COUNT)
        // значение всегда протухает до следующего опроса RPM и отображается как 0
        float   poll_time        = (can_metrics.rpm_poll_time_ts != 0) ? can_metrics.rpm_poll_time : 0.0f;
        float   battery_voltage  = can_value(can_metrics.battery_voltage,   can_metrics.battery_voltage_ts);
        float   transmission     = can_value(can_metrics.cvt_temp,          can_metrics.cvt_temp_ts);
#endif

        // Пока алерт активен — метрики не обновляем, чтобы не затирать оверлей
        if (!alert_manager.has_active_alert()) {
            display.update_metrics(coolant, oil, coolant_r,
                                   transmission, rpm,
                                   oil_pressure, boost,
                                   poll_time, battery_voltage);
        }

        // Индикатор журнала: красный кружок если есть хотя бы одна запись
        display.update_alert_indicator(alert_manager.log_count() > 0);

        // Оверлей алерта: показываем на весь экран или убираем при снятии алерта
        if (alert_manager.has_active_alert()) {
            display.show_alert(alert_manager.active_code(),
                               alert_manager.active_display_name());
        } else {
            display.clear_alert();
        }
    }

    state_unlock();
#endif
}
