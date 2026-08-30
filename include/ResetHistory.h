#pragma once

#include <Arduino.h>
#include <esp_system.h>

// Максимальное количество загрузок в постоянном журнале
static constexpr uint8_t RESET_HISTORY_MAX = 10;

class ResetHistory {
public:
    ResetHistory();

    // Загрузить журнал и добавить причину текущей загрузки
    bool init();

    // Вернуть причину текущей загрузки
    esp_reset_reason_t current_reason() const;

    // Вернуть строковое имя причины текущей загрузки
    const char *current_reason_name() const;

    // Сериализовать журнал от новой записи к старой
    String to_json() const;

    // Вернуть стабильное строковое имя причины загрузки
    static const char *reason_name(esp_reset_reason_t reason);

private:
    esp_reset_reason_t reasons_[RESET_HISTORY_MAX];
    uint8_t            count_;
    esp_reset_reason_t current_reason_;

    bool load_();
    bool save_() const;
};

extern ResetHistory reset_history;
