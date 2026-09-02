#pragma once

#include <stdint.h>

// Типы и декодер CAN, не зависящие от железа.
//
// Вынесены из CanBusManager.h намеренно: тот тянет <driver/twai.h> и разворачивает
// макросы ESP-IDF на уровне пространства имен, поэтому его нельзя подключить
// при сборке под хост. Здесь только структуры данных и чистая функция разбора,
// благодаря чему декодер и потребители метрик (AlertManager) покрываются
// юнит-тестами в окружении native

// Структура одного принятого CAN-фрейма
struct CanFrame {
    uint32_t id;        // Идентификатор фрейма (11 или 29 бит)
    bool     extended;  // true — расширенный (29-бит) идентификатор
    bool     rtr;       // true — запрос удаленной передачи (RTR-фрейм)
    uint8_t  dlc;       // Длина данных (0–8 байт)
    uint8_t  data[8];   // Данные фрейма
};

enum class MetricSource : uint8_t {
    NONE = 0,
    INFINITI_UDS = 1,
    OBD2 = 2,
};

// Метрики, декодированные из CAN-шины Infiniti QX50 J55.
// Каждое поле *_ts хранит millis() момента последнего обновления.
// Начальное значение всех полей — 0 (не получено ни одного фрейма).
struct CanMetrics {
    float    engine_coolant;       // Т ОЖ ДВС, °C (UDS DID 0x1101)
    uint32_t engine_coolant_ts;    // Время последнего обновления T ОЖ ДВС
    MetricSource engine_coolant_source;

    float    engine_oil;           // Т масла ДВС, °C (UDS DID 0x111F)
    uint32_t engine_oil_ts;        // Время последнего обновления T масла ДВС
    MetricSource engine_oil_source;

    float    radiator_coolant;     // Т ОЖ радиатора, °C (UDS DID 0x116B)
    uint32_t radiator_coolant_ts;  // Время последнего обновления T ОЖ радиатора

    float    engine_rpm;           // Обороты двигателя
    uint32_t engine_rpm_ts;        // Время последнего обновления оборотов двигателя
    MetricSource engine_rpm_source;

    float    turbo_boost_volt;     // Датчик усиления турбины
    uint32_t turbo_boost_volt_ts;  // Время последнего обновления датчика усиления турбины

    float oil_pressure_volt;       // Датчик давления масла ДВС
    uint32_t oil_pressure_volt_ts; // Время последнего обновления датчика давления масла ДВС

    float battery_voltage;         // Напряжение бортовой сети
    uint32_t battery_voltage_ts;   // Время последнего обновления напряжения бортовой сети
    MetricSource battery_voltage_source;

    float cvt_temp;                // Температура масла вариатора
    uint32_t cvt_temp_ts;          // Время последнего обновления температуры масла вариатора

    bool     exterior_light_on;    // Состояние внешнего света (UDS DID 0x0E07, бит 0x10)
    uint32_t exterior_light_ts;    // Время последнего обновления состояния света

    uint32_t rpm_request_ts;       // Момент отправки UDS-запроса оборотов (millis)
    // Период обновления оборотов, с — интервал между двумя отправками запроса RPM,
    // равен poll_interval_ms * POLL_COUNT. Это не время ответа ECU (0 = нет данных)
    float    rpm_poll_time;
    uint32_t rpm_poll_time_ts;     // Время последнего обновления rpm_poll_time
};

// Глобальный объект метрик — заполняется из can_parse_known_frames()
extern CanMetrics can_metrics;

// Декодировать известные CAN-фреймы Infiniti QX50 J55 и обновить can_metrics
// (обороты, температуры, наддув, давление масла, напряжение бортовой сети)
void can_parse_known_frames(const CanFrame &frame);
