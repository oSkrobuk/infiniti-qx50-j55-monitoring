#pragma once
#include <Arduino.h>
#include <driver/twai.h>

// Пин TX модуля SN65HVD230 (WVCMCU-230) → GPIO ESP32
// GPIO32 — стоит рядом с GPIO34 на плате, удобно для прокладки проводов CAN
static constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_32;
// Пин RX модуля SN65HVD230 (WVCMCU-230) → GPIO ESP32
// GPIO34 — только вход, не конфликтует с дисплеем (TFT_RST=GPIO4)
static constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_34;

// Скорость шины CAN (500 кбит/с — стандарт для большинства автомобилей)
static constexpr twai_timing_config_t CAN_TIMING = TWAI_TIMING_CONFIG_500KBITS();

// Структура одного принятого CAN-фрейма
struct CanFrame
{
    uint32_t id;        // Идентификатор фрейма (11 или 29 бит)
    bool     extended;  // true — расширенный (29-бит) идентификатор
    bool     rtr;       // true — запрос удалённой передачи (RTR-фрейм)
    uint8_t  dlc;       // Длина данных (0–8 байт)
    uint8_t  data[8];   // Данные фрейма
};

// Колбэк, вызываемый при получении каждого фрейма
using CanFrameCallback = std::function<void(const CanFrame &frame)>;

class CanBusManager
{
public:
    CanBusManager();

    // Инициализация TWAI-контроллера ESP32 и запуск шины.
    // Возвращает true при успехе.
    bool init();

    // Остановить и деинициализировать TWAI-контроллер.
    void stop();

    // Зарегистрировать колбэк, который будет вызываться для каждого
    // принятого фрейма. Можно зарегистрировать только один колбэк.
    void onFrame(CanFrameCallback cb);

    // Вызывать в loop(): читает все доступные фреймы из очереди TWAI
    // и передаёт их в зарегистрированный колбэк.
    void handle();

    // Вернуть количество успешно принятых фреймов с момента запуска
    uint32_t receivedCount() const;

    // Вернуть количество ошибок шины с момента запуска
    uint32_t errorCount() const;

    // Вернуть true, если контроллер запущен и шина активна
    bool isRunning() const;

private:
    bool             _running;
    uint32_t         _rxCount;
    uint32_t         _errCount;
    CanFrameCallback _callback;

    // Преобразовать twai_message_t → CanFrame
    static CanFrame _toFrame(const twai_message_t &msg);
};

// Глобальный объект, доступен из всех файлов
extern CanBusManager canBus;
