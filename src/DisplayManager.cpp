#include "DisplayManager.h"

#include "ConfigManager.h"

DisplayManager::DisplayManager()
    : tft_(TFT_eSPI()), alert_visible_(false)
{
    version_buf_[0]      = '\0';
    drawn_alert_code_[0] = '\0';
}

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
    tft_.init();
    tft_.setRotation(0);
    tft_.fillScreen(TFT_BLACK);

    if (version) {
        snprintf(version_buf_, sizeof(version_buf_), "Version %s", version);
    }

    draw_static_();
}

// ─────────────────────────────────────────────────────────────────────────────
// Алерт-оверлей: занимает ВЕСЬ экран, метрики в это время не обновляются

void DisplayManager::show_alert(const char *code, const char *description)
{
    // Перерисовываем только если код изменился
    if (alert_visible_ && strncmp(drawn_alert_code_, code, sizeof(drawn_alert_code_)) == 0) {
        return;
    }

    // Очищаем весь экран
    tft_.fillScreen(TFT_BLACK);

    // Код алерта — красный, font 4
    tft_.setTextColor(0xF800, TFT_BLACK);
    tft_.drawCentreString(code, 120, 20, 4);

    // Разделитель
    tft_.drawFastHLine(10, 58, 220, 0xF800);

    // Описание: разбиваем на строки по пробелу ближайшему к середине
    char line1[33] = "";
    char line2[33] = "";
    char line3[33] = "";
    char line4[33] = "";
    char line5[33] = "";
    int len = static_cast<int>(strlen(description));

    if (len <= 19) {
        // Короткий текст — всё в первую строку, остальные пустые
        strncpy(line1, description, 32); line1[31] = '\0';
        line2[0] = '\0'; line3[0] = '\0'; line4[0] = '\0'; line5[0] = '\0';
    } else {
        int lines_count = 5;
        int chunk_size = len / lines_count;
        int current_idx = 0;
        char* target_lines[5] = {line1, line2, line3, line4, line5};

        for (int l = 0; l < lines_count; ++l) {
            if (current_idx >= len) {
                target_lines[l][0] = '\0';
                continue;
            }

            // Для последней строки просто забираем весь остаток
            if (l == lines_count - 1) {
                strncpy(target_lines[l], description + current_idx, 32);
                target_lines[l][31] = '\0';
                break;
            }

            // Ищем оптимальную точку разреза (ближайший пробел вперед)
            int target_split = current_idx + chunk_size;
            int split_point = -1;

            for (int i = target_split; i < len; ++i) {
                if (description[i] == ' ') {
                    split_point = i;
                    break;
                }
            }

            // Если пробел впереди не найден, ищем его назад
            if (split_point < 0) {
                for (int i = target_split; i > current_idx; --i) {
                    if (description[i] == ' ') {
                        split_point = i;
                        break;
                    }
                }
            }

            // Если пробелов вообще нет, режем жестко по размеру куска
            if (split_point < 0) {
                split_point = target_split;
            }

            int copy_len = split_point - current_idx;
            if (copy_len > 31) copy_len = 31; // Защита от переполнения буфера в 32 байта

            strncpy(target_lines[l], description + current_idx, copy_len);
            target_lines[l][copy_len] = '\0';

            // Шаг вперед. Если разбились по пробелу, пропускаем его (+1)
            current_idx = split_point + (description[split_point] == ' ' ? 1 : 0);
        }
    }

    tft_.setTextColor(TFT_WHITE, TFT_BLACK);

    if (line5[0]) {
        // Пять строк (было 95...207)
        tft_.drawCentreString(line1, 120, 80, 4);
        tft_.drawCentreString(line2, 120, 108, 4);
        tft_.drawCentreString(line3, 120, 136, 4);
        tft_.drawCentreString(line4, 120, 164, 4);
        tft_.drawCentreString(line5, 120, 192, 4);
    } else if (line4[0]) {
        // Четыре строки (было 108...195)
        tft_.drawCentreString(line1, 120, 93, 4);
        tft_.drawCentreString(line2, 120, 122, 4);
        tft_.drawCentreString(line3, 120, 151, 4);
        tft_.drawCentreString(line4, 120, 180, 4);
    } else if (line3[0]) {
        // Три строки (было 121...181)
        tft_.drawCentreString(line1, 120, 106, 4);
        tft_.drawCentreString(line2, 120, 136, 4);
        tft_.drawCentreString(line3, 120, 166, 4);
    } else if (line2[0]) {
        // Две строки (было 135...167)
        tft_.drawCentreString(line1, 120, 120, 4);
        tft_.drawCentreString(line2, 120, 152, 4);
    } else {
        // Одна строка (было 150)
        tft_.drawCentreString(line1, 120, 135, 4);
    }

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
}

// ─────────────────────────────────────────────────────────────────────────────

uint16_t DisplayManager::get_temperature_color(float value, float min_temp,
                                               float target_temp, float max_temp)
{
    if (value >= max_temp) {
        return 0xF800;
    }
    if (value <= min_temp) {
        return 0x001F;
    }
    if (target_temp <= min_temp || max_temp <= target_temp) {
        return 0xFFFF;
    }

    float factor = 0.0f;
    float hue    = 120.0f;

    if (value < target_temp) {
        factor = (value - min_temp) / (target_temp - min_temp);
        hue    = 240.0f - (factor * 120.0f);
    } else {
        factor = (value - target_temp) / (max_temp - target_temp);
        hue    = 120.0f - (factor * 120.0f);
    }

    float h = hue / 60.0f;
    float x = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
    float r = 0, g = 0, b = 0;

    if (h >= 0 && h < 1) {
        r = 1; g = x; b = 0;
    } else if (h >= 1 && h < 2) {
        r = x; g = 1; b = 0;
    } else if (h >= 2 && h < 3) {
        r = 0; g = 1; b = x;
    } else {
        r = 0; g = x; b = 1;
    }

    return static_cast<uint16_t>(
        (static_cast<uint16_t>(r * 31) << 11) |
        (static_cast<uint16_t>(g * 63) <<  5) |
         static_cast<uint16_t>(b * 31));
}

uint16_t DisplayManager::get_rpm_color(float rpm)
{
    const float green_start = config.get("rpm", "green_start");
    const float green_end   = config.get("rpm", "green_end");
    const float red_start   = config.get("rpm", "red_start");

    if (rpm <= green_start) {
        if (rpm <= 750.0f) return 0x001F;

        float t   = (rpm - 750.0f) / (green_start - 750.0f);
        float hue = 240.0f - t * 120.0f;
        float h   = hue / 60.0f;
        float x   = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
        float r = 0, g = 0, b = 0;
        if (h >= 4.0f) { r = x; g = 0; b = 1; }
        else if (h >= 3.0f) { r = 0; g = x; b = 1; }
        else                { r = 0; g = 1; b = x; }
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(r * 31) << 11) |
            (static_cast<uint16_t>(g * 63) <<  5) |
             static_cast<uint16_t>(b * 31));
    }

    if (rpm <= green_end) return 0x07E0;
    if (rpm >= red_start) return 0xF800;

    float t   = (rpm - green_end) / (red_start - green_end);
    float hue = 120.0f - t * 120.0f;
    float h   = hue / 60.0f;
    float x   = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
    float r = 0, g = 0, b = 0;
    if (h < 1) { r = 1; g = x; b = 0; }
    else       { r = x; g = 1; b = 0; }
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(r * 31) << 11) |
        (static_cast<uint16_t>(g * 63) <<  5) |
         static_cast<uint16_t>(b * 31));
}

uint16_t DisplayManager::get_boost_color(float boost)
{
    const float blue_max  = config.get("boost", "blue_max");
    const float green_min = config.get("boost", "green_min");

    if (boost <= blue_max)  return 0x001F;
    if (boost >= green_min) return 0x07E0;

    float t   = (boost - blue_max) / (green_min - blue_max);
    float hue = 240.0f - t * 120.0f;
    float h   = hue / 60.0f;
    float x   = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
    float r = 0, g = 0, b = 0;
    if (h >= 4.0f) { r = x; g = 0; b = 1; }
    else if (h >= 3.0f) { r = 0; g = x; b = 1; }
    else                { r = 0; g = 1; b = x; }
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(r * 31) << 11) |
        (static_cast<uint16_t>(g * 63) <<  5) |
         static_cast<uint16_t>(b * 31));
}

uint16_t DisplayManager::get_poll_time_color(float poll_time, float rpm)
{
    if (rpm == 0.0f || poll_time == 0.0f) return 0x001F;

    const float green_max = config.get("poll_time", "green_max");
    const float red_min   = config.get("poll_time", "red_min");

    if (poll_time <= green_max) return 0x07E0;
    if (poll_time >= red_min)   return 0xF800;

    float t   = (poll_time - green_max) / (red_min - green_max);
    float hue = 120.0f - t * 120.0f;
    float h   = hue / 60.0f;
    float x   = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
    float rv = 0, gv = 0, bv = 0;
    if (h < 1.0f) { rv = 1; gv = x; bv = 0; }
    else          { rv = x; gv = 1; bv = 0; }
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(rv * 31) << 11) |
        (static_cast<uint16_t>(gv * 63) <<  5) |
         static_cast<uint16_t>(bv * 31));
}

uint16_t DisplayManager::get_battery_color(float voltage)
{
    if (voltage == 0.0f) return 0x001F;

    const float red_low   = config.get("battery", "red_low");
    const float green_min = config.get("battery", "green_min");
    const float green_max = config.get("battery", "green_max");
    const float red_high  = config.get("battery", "red_high");

    if (voltage < red_low)  return 0xF800;
    if (voltage > red_high) return 0xF800;
    if (voltage >= green_min && voltage <= green_max) return 0x07E0;

    if (voltage < green_min) {
        float t   = (voltage - red_low) / (green_min - red_low);
        float hue = t * 120.0f;
        float h   = hue / 60.0f;
        float x   = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
        float r = 0, g = 0, b = 0;
        if (h < 1) { r = 1; g = x; b = 0; }
        else       { r = x; g = 1; b = 0; }
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(r * 31) << 11) |
            (static_cast<uint16_t>(g * 63) <<  5) |
             static_cast<uint16_t>(b * 31));
    }

    {
        float t   = (voltage - green_max) / (red_high - green_max);
        float hue = 120.0f - t * 120.0f;
        float h   = hue / 60.0f;
        float x   = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
        float r = 0, g = 0, b = 0;
        if (h < 1) { r = 1; g = x; b = 0; }
        else       { r = x; g = 1; b = 0; }
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(r * 31) << 11) |
            (static_cast<uint16_t>(g * 63) <<  5) |
             static_cast<uint16_t>(b * 31));
    }
}

uint16_t DisplayManager::get_oil_pressure_color(float pressure, float rpm)
{
    if (pressure == 0.0f) return 0x001F;

    float threshold    = config.get("oil_pressure", "rpm_threshold");
    float min_pressure = (rpm < threshold)
        ? config.get("oil_pressure", "min_low")
        : config.get("oil_pressure", "min_high");

    return (pressure < min_pressure) ? 0xF800 : 0x07E0;
}

void DisplayManager::update_metrics(float coolant, float oil, float coolant_r,
                                    float transmission, float rpm,
                                    float oil_pressure, float boost,
                                    float poll_time, float battery_voltage)
{
    char buf[12];

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

    // Давление масла
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

    // Время опроса RPM
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
