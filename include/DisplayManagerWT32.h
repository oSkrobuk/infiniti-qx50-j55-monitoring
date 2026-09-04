#pragma once

#include <Arduino.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Менеджер дисплея для WT32-SC01 Plus (ESP32-S3)
// Дисплей: ST7796, 320×480, 8-битная параллельная шина (MCU 8080)
// Интерфейс полностью идентичен DisplayManager для ESP32 DEVKIT1

class Wt32Display : public lgfx::LGFX_Device {
public:
    Wt32Display();

    int32_t text_width(const char *text, uint8_t font);

private:
    lgfx::Panel_ST7796 panel_;
    lgfx::Bus_Parallel8 bus_;
    lgfx::Light_PWM light_;
};

class DisplayManagerWT32 {
public:
    DisplayManagerWT32();

    void init(const char *version = nullptr);

    uint16_t get_temperature_color(float value, float min_temp,
                                   float target_temp, float max_temp);

    // Цвет оборотов: синий(<green_start) → зелёный → жёлтый → красный(≥red_start)
    uint16_t get_rpm_color(float rpm);

    // Цвет давления масла: красный если ниже минимума для текущих оборотов
    uint16_t get_oil_pressure_color(float pressure, float rpm);

    // Цвет наддува: синий(≤blue_max) → жёлтый → зелёный(≥green_min)
    uint16_t get_boost_color(float boost);

    // Цвет вольтажа: красный(<red_low или >red_high) → жёлтый → зелёный(green_min..green_max)
    uint16_t get_battery_color(float voltage);

    // Цвет времени опроса RPM: синий(rpm=0) → зелёный(≤0.2с) → красный(≥0.5с)
    uint16_t get_poll_time_color(float poll_time, float rpm);

    void update_metrics(float coolant, float oil, float coolant_r,
                        float transmission, float rpm,
                        float oil_pressure, float boost,
                        float poll_time, float battery_voltage);

    // Показать алерт-оверлей поверх заголовка экрана
    // display_name — строки через '\n': первая и третья UPPERCASE красные, вторая белая
    // Вызывать каждый кадр пока алерт активен — перерисовывает только при смене кода
    void show_alert(const char *code, const char *display_name);

    // Убрать алерт-оверлей, восстановить заголовок экрана
    // Вызывать один раз когда алерт перестал быть активным
    void clear_alert();

    // Обновить индикатор алертов (красный кружок слева от «MONITORING»)
    // has_alerts — true если в журнале есть хотя бы одна запись
    void update_alert_indicator(bool has_alerts);

private:
    static constexpr uint8_t METRIC_COUNT = 9;
    static constexpr size_t METRIC_VALUE_SIZE = 12;

    Wt32Display tft_;

    // Буфер версии прошивки, сохраняется для восстановления после очистки оверлея
    char version_buf_[32];

    // Код последнего нарисованного алерта — для предотвращения лишних перерисовок
    char drawn_alert_code_[8];

    // true если в данный момент на экране показан алерт-оверлей
    bool alert_visible_;

    // Текущее состояние индикатора алертов — для предотвращения лишних перерисовок
    bool alert_indicator_;

    // Текущее заполнение PWM подсветки — для предотвращения лишних записей
    uint8_t brightness_duty_;

    // Последние нарисованные значения и цвета — для пропуска неизменившихся плиток
    char metric_value_cache_[METRIC_COUNT][METRIC_VALUE_SIZE];
    uint16_t metric_color_cache_[METRIC_COUNT];
    bool metric_cache_valid_[METRIC_COUNT];

    // Перерисовать все статические элементы экрана (заголовок, подписи, версия)
    // Используется при init() и при clear_alert() для полного восстановления UI
    void draw_static_();

    // Перерисовать статический заголовок (INFINITI QX50 J55 / MONITORING)
    // Используется при init() и при clear_alert()
    void draw_header_();

    // Нарисовать статическую часть плитки метрики
    void draw_metric_tile_(uint8_t index, const char *label, const char *unit);

    // Обновить цвет состояния и значение внутри плитки метрики
    void draw_metric_value_(uint8_t index, const char *value, uint16_t color);

    // Обновить плитку только при изменении текста или цвета
    void update_metric_value_(uint8_t index, const char *value, uint16_t color);

    // Сбросить кеш после полной перерисовки экрана
    void invalidate_metric_cache_();

    // Нарисовать сглаженный полужирный текст через масштабируемый спрайт
    bool draw_smooth_text_(const char *text, int16_t center_x, int16_t center_y,
                           uint16_t color, float zoom, int16_t sprite_width);

    // Нарисовать значение вместе с фоном одним кадром без мерцания
    bool draw_smooth_value_(const char *text, int16_t center_x, int16_t center_y,
                            uint16_t color);

    // Обновить яркость подсветки из системной конфигурации
    void update_brightness_();
};
