#pragma once
#include <Arduino.h>
#include <driver/twai.h>

#include "CanTypes.h"

// Пины CAN-модуля SN65HVD230 (WVCMCU-230) задаются отдельно для каждой платы через build_flags
static constexpr gpio_num_t CAN_TX_PIN = static_cast<gpio_num_t>(CAN_TX_PIN_NUM);
static constexpr gpio_num_t CAN_RX_PIN = static_cast<gpio_num_t>(CAN_RX_PIN_NUM);

// Скорость шины CAN (500 кбит/с — стандарт для большинства автомобилей)
static constexpr twai_timing_config_t CAN_TIMING = TWAI_TIMING_CONFIG_500KBITS();

// Структуры CanFrame и CanMetrics, а также декодер can_parse_known_frames()
// объявлены в CanTypes.h — там нет зависимостей от TWAI, поэтому они доступны
// в юнит-тестах под хостом

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

    // Вернуть количество переходов контроллера в Bus-Off
    uint32_t bus_off_count() const;

    // Вернуть количество успешных перезапусков TWAI после Bus-Off
    uint32_t recovery_count() const;

    // Вернуть время последнего принятого CAN-кадра
    uint32_t last_rx_ts() const;

    // Вернуть время последнего ответа блока двигателя
    uint32_t last_ecm_response_ts() const;

    // Вернуть время последнего ответа блока трансмиссии
    uint32_t last_tcm_response_ts() const;

    // Вернуть текущее состояние контроллера в виде строки для диагностики
    const char *state_name() const;

    // Вернуть true, если контроллер запущен и шина активна
    bool is_running() const;

private:
    bool             driver_installed_;
    bool             running_;
    uint32_t         rx_count_;
    uint32_t         err_count_;
    uint32_t         bus_off_count_;
    uint32_t         recovery_count_;
    uint32_t         last_rx_ts_;
    uint32_t         last_ecm_response_ts_;
    uint32_t         last_tcm_response_ts_;
    twai_state_t     state_;
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
