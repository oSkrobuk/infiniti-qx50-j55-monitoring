#include "DisplayManager.h"

#include "ConfigManager.h"
#include "MetricColors.h"

DisplayManager::DisplayManager()
    : tft_(TFT_eSPI()), alert_visible_(false), alert_indicator_(false), brightness_duty_(0)
{
    version_buf_[0]      = '\0';
    drawn_alert_code_[0] = '\0';
}

// Пин BLK внешнего ST7789 на ESP32 DEVKIT1
static constexpr uint8_t DISPLAY_BL_PIN = 27;
static constexpr uint8_t DISPLAY_BL_LEDC_CHANNEL = 2;
static constexpr uint32_t DISPLAY_BL_LEDC_FREQ_HZ = 5000;
static constexpr uint8_t DISPLAY_BL_LEDC_BITS = 8;
static constexpr uint8_t DISPLAY_BL_MAX_DUTY = 255;
static constexpr float BRIGHTNESS_MIN_PERCENT = 10.0f;
static constexpr float BRIGHTNESS_MAX_PERCENT = 100.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Внутренний метод: рисует все статические заголовки и лейблы
// Вызывается при init() и при clear_alert() для восстановления интерфейса

void DisplayManager::draw_static_()
{
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString("INFINITI QX50 J55", 30, 5, 4);
    tft_.setTextColor(0xCE70, TFT_BLACK);
    tft_.drawString("MONITORING", 75, 28, 4);

    // Секция температуры
    tft_.drawFastHLine(0, 59, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" TEMPERATURE, C ", 120, 51, 2);
    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawCentreString("RAD-ANT", 40, 69, 2);
    tft_.drawCentreString("ENG-ANT", 120, 69, 2);
    tft_.drawCentreString("ENG-OIL", 200, 69, 2);

    // Секция двигателя
    tft_.drawFastHLine(0, 118, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" ENGINE ", 120, 110, 2);
    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawCentreString("ENG-RPM",  40, 128, 2);
    tft_.drawCentreString("OIL-PR-V", 120, 128, 2);
    tft_.drawCentreString("TURBO-V",  200, 128, 2);

    // Секция прочего
    tft_.drawFastHLine(0, 177, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" OTHER ", 120, 169, 2);
    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawCentreString("BATTERY-V", 40, 187, 2);
    tft_.drawCentreString("RPM-POLL",  125, 187, 2);
    tft_.drawCentreString("CVT-FLD",   200, 187, 2);

    // Версия прошивки внизу
    if (version_buf_[0]) {
        tft_.setTextColor(0xCE70, TFT_BLACK);
        tft_.drawCentreString(version_buf_, 120, 232, 1);
    }
}

// Внутренний метод: перерисовывает только заголовок (верхние 60px)
// Оставлен для совместимости; используется draw_static_ при полном сбросе

void DisplayManager::draw_header_()
{
    tft_.fillRect(0, 0, 240, 90, TFT_BLACK);

    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString("INFINITI QX50 J55", 30, 5, 4);
    tft_.setTextColor(0xCE70, TFT_BLACK);
    tft_.drawString("MONITORING", 75, 28, 4);

    tft_.drawFastHLine(0, 59, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" TEMPERATURE, C ", 120, 51, 2);

    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawCentreString("RAD-ANT", 40, 69, 2);
    tft_.drawCentreString("ENG-ANT", 120, 69, 2);
    tft_.drawCentreString("ENG-OIL", 200, 69, 2);
}

void DisplayManager::init(const char *version)
{
    ledcSetup(DISPLAY_BL_LEDC_CHANNEL, DISPLAY_BL_LEDC_FREQ_HZ, DISPLAY_BL_LEDC_BITS);
    ledcAttachPin(DISPLAY_BL_PIN, DISPLAY_BL_LEDC_CHANNEL);
    update_brightness_();

    tft_.init();
    // TFT_eSPI принимает 0..3 и сам берет остаток от деления на 4:
    // раньше здесь стояло 90, что давало ту же ориентацию 90 % 4 = 2
    tft_.setRotation(2);
    tft_.fillScreen(TFT_BLACK);

    if (version) {
        snprintf(version_buf_, sizeof(version_buf_), "Version %s", version);
    }

    draw_static_();
}

void DisplayManager::update_brightness_()
{
    float brightness_percent = config.get("system", "brightness_percent");
    if (brightness_percent < BRIGHTNESS_MIN_PERCENT) {
        brightness_percent = BRIGHTNESS_MIN_PERCENT;
    } else if (brightness_percent > BRIGHTNESS_MAX_PERCENT) {
        brightness_percent = BRIGHTNESS_MAX_PERCENT;
    }

    const uint8_t duty = static_cast<uint8_t>(
        ((brightness_percent / 100.0f) * DISPLAY_BL_MAX_DUTY) + 0.5f);

    if (duty != brightness_duty_) {
        brightness_duty_ = duty;
        ledcWrite(DISPLAY_BL_LEDC_CHANNEL, brightness_duty_);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Алерт-оверлей: занимает ВЕСЬ экран, метрики в это время не обновляются

void DisplayManager::show_alert(const char *code, const char *display_name)
{
    // Перерисовываем только если код изменился
    if (alert_visible_ && strncmp(drawn_alert_code_, code, sizeof(drawn_alert_code_)) == 0) {
        return;
    }

    // Очищаем весь экран
    tft_.fillScreen(TFT_BLACK);

    // Код алерта — золотой, font 4, вверху
    tft_.setTextColor(0xCE70, TFT_BLACK);
    tft_.drawCentreString(code, 120, 15, 4);

    // Разделительная линия
    tft_.drawFastHLine(10, 52, 220, 0xCE70);

    // Разбиваем display_name по '\n' на три строки (максимум 3)
    // Формат: "LINE1\nLine2\nLINE3"
    char line1[32] = "";
    char line2[32] = "";
    char line3[32] = "";

    const char *p = display_name;
    char *targets[3] = {line1, line2, line3};
    for (int i = 0; i < 3 && p && *p; ++i) {
        const char *nl = strchr(p, '\n');
        int copy_len;
        if (nl) {
            copy_len = static_cast<int>(nl - p);
        } else {
            copy_len = static_cast<int>(strlen(p));
        }
        if (copy_len > 31) copy_len = 31;
        // Используем memcpy вместо strncpy — длина уже точно известна и ограничена
        memcpy(targets[i], p, static_cast<size_t>(copy_len));
        targets[i][copy_len] = '\0';
        p = nl ? nl + 1 : nullptr;
    }

    // Позиционирование: область ниже разделителя y=52 до y=240 (188 px)
    // Font 4 — высота ~26 px, отступ между строками 16 px
    // Блок из 3 строк: 3*26 + 2*16 = 110 px
    // Центр блока: 52 + 188/2 = 146, верх блока: 146 - 55 = 91
    static constexpr int16_t LINE_H  = 26; // высота шрифта 4
    static constexpr int16_t LINE_GAP = 16; // отступ между строками
    static constexpr int16_t Y_START  = 91; // верх первой строки

    // Строка 1: красная (тема — например "ENGINE OIL")
    tft_.setTextColor(0xF800, TFT_BLACK);
    tft_.drawCentreString(line1, 120, Y_START, 4);

    // Строка 2: белая (параметр — например "Temperature")
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawCentreString(line2, 120, Y_START + LINE_H + LINE_GAP, 4);

    // Строка 3: красная (состояние — например "HIGH")
    tft_.setTextColor(0xF800, TFT_BLACK);
    tft_.drawCentreString(line3, 120, Y_START + (LINE_H + LINE_GAP) * 2, 4);

    strncpy(drawn_alert_code_, code, sizeof(drawn_alert_code_) - 1);
    drawn_alert_code_[sizeof(drawn_alert_code_) - 1] = '\0';
    alert_visible_ = true;
}

void DisplayManager::clear_alert()
{
    if (!alert_visible_) return;

    // Закрашиваем весь экран чёрным, затем восстанавливаем интерфейс
    tft_.fillScreen(TFT_BLACK);
    draw_static_();

    drawn_alert_code_[0] = '\0';
    alert_visible_       = false;

    // Экран полностью перерисован — сбрасываем кэш индикатора,
    // чтобы update_alert_indicator() восстановил кружок в следующем кадре
    alert_indicator_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────

void DisplayManager::update_alert_indicator(bool has_alerts)
{
    // Перерисовываем только при изменении состояния
    if (has_alerts == alert_indicator_) return;
    alert_indicator_ = has_alerts;

    // Кружок диаметром 8 px слева от строки «MONITORING» (y=28, font 4 ~ высота 26 px)
    static constexpr int16_t IND_X = 15;
    static constexpr int16_t IND_Y = 40;
    static constexpr int16_t IND_R = 8;

    if (has_alerts) {
        tft_.fillCircle(IND_X, IND_Y, IND_R, 0xF800); // красный
    } else {
        tft_.fillCircle(IND_X, IND_Y, IND_R, TFT_BLACK); // стираем
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Цветовые зоны вынесены в MetricColors.cpp — там нет зависимости от TFT_eSPI,
// поэтому вся арифметика покрыта юнит-тестами в окружении native

uint16_t DisplayManager::get_temperature_color(float value, float min_temp,
                                               float target_temp, float max_temp)
{
    return metric_temperature_color(value, min_temp, target_temp, max_temp);
}

uint16_t DisplayManager::get_rpm_color(float rpm)
{
    return metric_rpm_color(rpm);
}

uint16_t DisplayManager::get_boost_color(float boost)
{
    return metric_boost_color(boost);
}

uint16_t DisplayManager::get_poll_time_color(float poll_time, float rpm)
{
    return metric_poll_time_color(poll_time, rpm);
}

uint16_t DisplayManager::get_battery_color(float voltage)
{
    return metric_battery_color(voltage);
}

uint16_t DisplayManager::get_oil_pressure_color(float pressure, float rpm)
{
    return metric_oil_pressure_color(pressure, rpm);
}

// ─────────────────────────────────────────────────────────────────────────────

void DisplayManager::update_metrics(float coolant, float oil, float coolant_r,
                                    float transmission, float rpm,
                                    float oil_pressure, float boost,
                                    float poll_time, float battery_voltage)
{
    char buf[12];

    update_brightness_();

    // Антифриз радиатора
    uint16_t radiator_color = get_temperature_color(coolant_r,
        config.get("radiator", "min"),
        config.get("radiator", "target"),
        config.get("radiator", "max"));
    tft_.setTextColor(radiator_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", coolant_r);
    tft_.drawString(buf, 20, 87, 4);

    // Антифриз ДВС
    uint16_t coolant_color = get_temperature_color(coolant,
        config.get("coolant", "min"),
        config.get("coolant", "target"),
        config.get("coolant", "max"));
    tft_.setTextColor(coolant_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", coolant);
    tft_.drawString(buf, 100, 87, 4);

    // Моторное масло
    uint16_t oil_color = get_temperature_color(oil,
        config.get("oil", "min"),
        config.get("oil", "target"),
        config.get("oil", "max"));
    tft_.setTextColor(oil_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", oil);
    tft_.drawString(buf, 180, 87, 4);

    // Обороты двигателя
    tft_.setTextColor(get_rpm_color(rpm), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("6000", 4));
    snprintf(buf, sizeof(buf), "%.0f", rpm);
    tft_.drawString(buf, 12, 146, 4);
    tft_.setTextPadding(0);

    // Напряжение датчика давления масла
    tft_.setTextColor(get_oil_pressure_color(oil_pressure, rpm), TFT_BLACK);
    snprintf(buf, sizeof(buf), "%.2f", oil_pressure);
    tft_.drawString(buf, 95, 146, 4);

    // Давление наддува
    tft_.setTextColor(get_boost_color(boost), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("-0.50", 4));
    snprintf(buf, sizeof(buf), "%.2f", boost);
    tft_.drawString(buf, 175, 146, 4);
    tft_.setTextPadding(0);

    // Вольтаж бортовой сети
    tft_.setTextColor(get_battery_color(battery_voltage), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("14.99", 4));
    snprintf(buf, sizeof(buf), "%.2f", battery_voltage);
    tft_.drawString(buf, 7, 205, 4);
    tft_.setTextPadding(0);

    // Период обновления RPM
    tft_.setTextColor(get_poll_time_color(poll_time, rpm), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("0.60", 4));
    if (rpm == 0.0f || poll_time == 0.0f) {
        snprintf(buf, sizeof(buf), "0");
    } else {
        snprintf(buf, sizeof(buf), "%.2f", poll_time);
    }
    tft_.drawString(buf, 95, 205, 4);
    tft_.setTextPadding(0);

    // Масло коробки
    uint16_t transmission_color = get_temperature_color(transmission,
        config.get("transmission", "min"),
        config.get("transmission", "target"),
        config.get("transmission", "max"));
    tft_.setTextColor(transmission_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", transmission);
    tft_.drawString(buf, 180, 205, 4);
}
