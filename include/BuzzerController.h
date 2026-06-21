#pragma once
#include <Arduino.h>

constexpr uint8_t CAN_BUZZER_PIN = 26;

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
