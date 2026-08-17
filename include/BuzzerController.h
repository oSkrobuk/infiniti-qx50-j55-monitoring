#pragma once
#include <Arduino.h>

// Пин внешнего пассивного бузера задаётся отдельно для каждой платы через build_flags
static constexpr uint8_t CAN_BUZZER_PIN = static_cast<uint8_t>(BUZZER_PIN_NUM);

class BuzzerController
{
public:
    // Запускаем оповещение о проблеме
    void trigger_alert();

    // Функция, которая постоянно запускается в loop
    void update();

    // Звук приветствия как при загрузке BIOS
    void hello();

private:
    bool is_playing_ = false;
    int step_ = 0;
    unsigned long last_time_ = 0;
};
