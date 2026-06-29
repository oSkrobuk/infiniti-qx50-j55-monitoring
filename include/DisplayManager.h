#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
public:
    DisplayManager();

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
    TFT_eSPI tft_;

    // Буфер версии прошивки, сохраняется для восстановления после очистки оверлея
    char version_buf_[32];

    // Код последнего нарисованного алерта — для предотвращения лишних перерисовок
    char drawn_alert_code_[8];

    // true если в данный момент на экране показан алерт-оверлей
    bool alert_visible_;

    // Текущее состояние индикатора алертов — для предотвращения лишних перерисовок
    bool alert_indicator_;

    // Перерисовать все статические элементы экрана (заголовок, подписи, версия)
    // Используется при init() и при clear_alert() для полного восстановления UI
    void draw_static_();

    // Перерисовать статический заголовок (INFINITI QX50 J55 / MONITORING)
    // Используется при init() и при clear_alert()
    void draw_header_();
};
