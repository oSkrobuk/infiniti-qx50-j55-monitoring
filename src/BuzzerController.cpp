#include "BuzzerController.h"

#include <Arduino.h>

// LEDC-канал для бузера (0..15 доступны на ESP32; 0 не занят TFT_eSPI)
static constexpr uint8_t BUZZER_LEDC_CHANNEL = 0;

// Частота и разрядность ШИМ
static constexpr uint32_t BUZZER_LEDC_FREQ_HZ  = 2700;
static constexpr uint8_t  BUZZER_LEDC_BITS      = 10;

static bool s_ledc_attached = false;

// Инициализация LEDC один раз — предотвращает ошибку "LEDC is not initialized",
// которую вызывает noTone() через ledcDetachPin() в Core 2.x.
// После этого используем только ledcWriteTone/ledcWrite без detach.
static void buzzer_attach()
{
    if (!s_ledc_attached) {
        ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_LEDC_FREQ_HZ, BUZZER_LEDC_BITS);
        ledcAttachPin(CAN_BUZZER_PIN, BUZZER_LEDC_CHANNEL);
        s_ledc_attached = true;
    }
}

static void buzzer_tone(uint32_t freq)
{
    buzzer_attach();
    ledcWriteTone(BUZZER_LEDC_CHANNEL, freq);
}

static void buzzer_off()
{
    // Duty = 0 → тишина, канал остаётся прикреплённым
    if (s_ledc_attached) {
        ledcWrite(BUZZER_LEDC_CHANNEL, 0);
    }
}

void BuzzerController::trigger_alert()
{
    if (!is_playing_) {
        is_playing_ = true;
        step_ = 0;
        last_time_ = millis();
    }
}

void BuzzerController::update()
{
    if (!is_playing_) {
        return;
    }

    unsigned long now = millis();

    // 1-й писк (Частота 2700 Гц, длительность 60 мс)
    if (step_ == 0) {
        buzzer_tone(2700);
        step_ = 1;
        last_time_ = now;
    }
    // Пауза после 1-го писка (50 мс)
    else if (step_ == 1 && (now - last_time_ >= 60)) {
        buzzer_off();
        step_ = 2;
        last_time_ = now;
    }
    // 2-й писк (60 мс)
    else if (step_ == 2 && (now - last_time_ >= 50)) {
        buzzer_tone(2700);
        step_ = 3;
        last_time_ = now;
    }
    // Пауза после 2-го писка (50 мс)
    else if (step_ == 3 && (now - last_time_ >= 60)) {
        buzzer_off();
        step_ = 4;
        last_time_ = now;
    }
    // 3-й писк (60 мс)
    else if (step_ == 4 && (now - last_time_ >= 50)) {
        buzzer_tone(2700);
        step_ = 5;
        last_time_ = now;
    }
    // Выключение звука
    else if (step_ == 5 && (now - last_time_ >= 60)) {
        buzzer_off();
        is_playing_ = false;
    }
}

void BuzzerController::hello()
{
    buzzer_tone(2500);
    delay(100);
    buzzer_off();
}
