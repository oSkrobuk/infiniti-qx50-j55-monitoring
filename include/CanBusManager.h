#pragma once
#include <Arduino.h>
#include <driver/twai.h>

// Пины CAN-модуля SN65HVD230 (WVCMCU-230) — задаются через build_flags в platformio.ini:
// CAN_TX_PIN_NUM=32, CAN_RX_PIN_NUM=34 (по умолчанию для ESP32 DEVKIT1 и WT32-SC01 Plus)
static constexpr gpio_num_t CAN_TX_PIN = static_cast<gpio_num_t>(CAN_TX_PIN_NUM);
static constexpr gpio_num_t CAN_RX_PIN = static_cast<gpio_num_t>(CAN_RX_PIN_NUM);

// Скорость шины CAN (500 кбит/с — стандарт для большинства автомобилей)
static constexpr twai_timing_config_t CAN_TIMING = TWAI_TIMING_CONFIG_500KBITS();

// Структура одного принятого CAN-фрейма
struct CanFrame {
    uint32_t id;        // Идентификатор фрейма (11 или 29 бит)
    bool     extended;  // true — расширенный (29-бит) идентификатор
    bool     rtr;       // true — запрос удалённой передачи (RTR-фрейм)
    uint8_t  dlc;       // Длина данных (0–8 байт)
    uint8_t  data[8];   // Данные фрейма
};

// Метрики, декодированные из CAN-шины Infiniti QX50 J55.
// Каждое поле *_ts хранит millis() момента последнего обновления.
// Начальное значение всех полей — 0 (не получено ни одного фрейма).
struct CanMetrics {
    float    engine_coolant;       // Т ОЖ ДВС, °C (UDS DID 0x1101)
    uint32_t engine_coolant_ts;    // Время последнего обновления T ОЖ ДВС

    float    engine_oil;           // Т масла ДВС, °C (UDS DID 0x111F)
    uint32_t engine_oil_ts;        // Время последнего обновления T масла ДВС

    float    radiator_coolant;     // Т ОЖ радиатора, °C (UDS DID 0x116B)
    uint32_t radiator_coolant_ts;  // Время последнего обновления T ОЖ радиатора

    float    engine_rpm;           // Обороты двигателя
    uint32_t engine_rpm_ts;        // Время последнего обновления оборотов двигателя

    float    turbo_boost_volt;     // Датчик усиления турбины
    uint32_t turbo_boost_volt_ts;  // Время последнего обновления датчика усиления турбины

    float oil_pressure_volt;       // Датчик давления масла ДВС
    uint32_t oil_pressure_volt_ts; // Время последнего обновления датчика давления масла ДВС

    float battery_voltage;         // Напряжение бортовой сети
    uint32_t battery_voltage_ts;   // Время последнего обновления напряжения бортовой сети

    float cvt_temp;                // Температура масла вариатора
    uint32_t cvt_temp_ts;          // Время последнего обновления температуры масла вариатора

    uint32_t rpm_request_ts;       // Момент отправки UDS-запроса оборотов (millis)
    float    rpm_poll_time;        // Время ответа на запрос оборотов, с (0 = нет данных)
    uint32_t rpm_poll_time_ts;     // Время последнего обновления rpm_poll_time
};

// Глобальный объект метрик — заполняется из can_parse_known_frames()
extern CanMetrics can_metrics;

// Колбэк, вызываемый при получении каждого фрейма
using CanFrameCallback = std::function<void(const CanFrame &frame)>;

class CanBusManager {
public:
    CanBusManager();

    // Инициализация TWAI-контроллера ESP32 и запуск шины
    // Возвращает true при успехе
    bool init();

    // Остановить и деинициализировать TWAI-контроллер
    void stop();

    // Зарегистрировать колбэк, который будет вызываться для каждого
    // принятого фрейма. Можно зарегистрировать только один колбэк
    void on_frame(CanFrameCallback cb);

    // Отправить CAN-фрейм в шину (только когда контроллер запущен)
    // Возвращает true если фрейм помещён в очередь TWAI
    bool send_frame(uint32_t id, const uint8_t *data, uint8_t dlc);

    // Вызывать в loop(): читает все доступные фреймы из очереди TWAI
    // и передаёт их в зарегистрированный колбэк
    void handle();

    // Вернуть количество успешно принятых фреймов с момента запуска
    uint32_t received_count() const;

    // Вернуть количество ошибок шины с момента запуска
    uint32_t error_count() const;

    // Вернуть true, если контроллер запущен и шина активна
    bool is_running() const;

private:
    bool             running_;
    uint32_t         rx_count_;
    uint32_t         err_count_;
    CanFrameCallback callback_;

    // Преобразовать twai_message_t → CanFrame
    static CanFrame to_frame(const twai_message_t &msg);
};

// Глобальный объект, доступен из всех файлов
extern CanBusManager can_bus;

// Вывести содержимое фрейма в Serial в читаемом виде:
// HEX, DEC, u16/i16 BE+LE, u32 BE+LE, f32 LE для каждого смещения
// Используется как колбэк по умолчанию для анализа шины
void can_print_frame(const CanFrame &frame);

// Декодировать известные CAN-фреймы Infiniti QX50 J55 и вывести
// человекочитаемые значения в Serial (обороты, температуры, передача, наддув)
void can_parse_known_frames(const CanFrame &frame);
