#include "BuzzerController.h"

void BuzzerController::trigger_alert()
{
    if (!is_playing_)
    {
        is_playing_ = true;
        step_ = 0;
        last_time_ = millis();
    }
}

void BuzzerController::update()
{
    if (!is_playing_)
    {
        return;
    }

    unsigned long now = millis();

    // 1-й писк (Частота 2700 Гц, длительность 60 мс)
    if (step_ == 0)
    {
        tone(CAN_BUZZER_PIN, 2700);
        step_ = 1;
        last_time_ = now;
    }
    // Пауза после 1-го писка (50 мс)
    else if (step_ == 1 && (now - last_time_ >= 60))
    {
        noTone(CAN_BUZZER_PIN);
        step_ = 2;
        last_time_ = now;
    }
    // 2-й писк (60 мс)
    else if (step_ == 2 && (now - last_time_ >= 50))
    {
        tone(CAN_BUZZER_PIN, 2700);
        step_ = 3;
        last_time_ = now;
    }
    // Пауза после 2-го писка (50 мс)
    else if (step_ == 3 && (now - last_time_ >= 60))
    {
        noTone(CAN_BUZZER_PIN);
        step_ = 4;
        last_time_ = now;
    }
    // 3-й писк (60 мс)
    else if (step_ == 4 && (now - last_time_ >= 50))
    {
        tone(CAN_BUZZER_PIN, 2700);
        step_ = 5;
        last_time_ = now;
    }
    // Выключение звука
    else if (step_ == 5 && (now - last_time_ >= 60))
    {
        noTone(CAN_BUZZER_PIN);
        is_playing_ = false;
    }
}

void BuzzerController::hello()
{
    // Включаем звук BIOS (частота 2500 Гц)
    tone(CAN_BUZZER_PIN, 2500);

    // Держим звук 100 миллисекунд
    delay(100);

    // Выключаем бузер
    noTone(CAN_BUZZER_PIN);
}
