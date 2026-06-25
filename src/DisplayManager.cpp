#include "DisplayManager.h"

#include "ConfigManager.h"

DisplayManager::DisplayManager()
    : tft_(TFT_eSPI()), alert_visible_(false)
{
    version_buf_[0]      = '\0';
    drawn_alert_code_[0] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// Внутренний метод: рисует статический заголовок (2 строки + секции)
// Вызывается при init() и при clear_alert() для восстановления

void DisplayManager::draw_header_()
{
    // Область заголовка — очищаем фоном
    tft_.fillRect(0, 0, 240, 60, TFT_BLACK);

    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString("INFINITI QX50 J55", 30, 5, 4);
    tft_.setTextColor(0xCE70, TFT_BLACK);
    tft_.drawString("MONITORING", 75, 28, 4);

    // Разделитель секции температуры
    tft_.drawFastHLine(0, 59, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" TEMPERATURE, C ", 120, 51, 2);
}

void DisplayManager::init(const char *version)
{
    tft_.init();
    tft_.setRotation(0);
    tft_.fillScreen(TFT_BLACK);

    draw_header_();

    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawCentreString("RAD-ANT", 40, 69, 2);
    tft_.drawCentreString("ENG-ANT", 120, 69, 2);
    tft_.drawCentreString("ENG-OIL", 200, 69, 2);

    // Двигатель
    tft_.drawFastHLine(0, 118, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" ENGINE ", 120, 110, 2);

    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawCentreString("ENG-RPM",  40, 128, 2);
    tft_.drawCentreString("OIL-PR-V", 120, 128, 2);
    tft_.drawCentreString("TURBO-V",  200, 128, 2);

    // Вариатор
    tft_.drawFastHLine(0, 177, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" OTHER ", 120, 169, 2);

    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawCentreString("BATTERY-V", 40, 187, 2);
    tft_.drawCentreString("RPM-POLL",  125, 187, 2);
    tft_.drawCentreString("CVT-FLD",   200, 187, 2);

    // Версия прошивки — мелким шрифтом внизу экрана
    if (version) {
        snprintf(version_buf_, sizeof(version_buf_), "Version %s", version);
        tft_.setTextColor(0xCE70, TFT_BLACK);
        tft_.drawCentreString(version_buf_, 120, 232, 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Алерт-оверлей: занимает верхние 57 пикселей (область заголовка)

void DisplayManager::show_alert(const char *code, const char *description)
{
    // Перерисовываем только если код изменился (чтобы не мигал каждые 100 мс)
    if (alert_visible_ && strncmp(drawn_alert_code_, code, sizeof(drawn_alert_code_)) == 0) {
        return;
    }

    // Тёмно-красный фон на всю ширину в области заголовка
    tft_.fillRect(0, 0, 240, 57, 0x4000); // тёмно-красный (RGB565)

    // Красная рамка
    tft_.drawRect(1, 1, 238, 55, 0xF800);

    // Код алерта — крупно слева + описание с переносом строки (font 2)
    tft_.setTextColor(TFT_WHITE, 0x4000);
    tft_.drawString(code, 8, 7, 4); // font 4 ≈ 26 px высота

    // Укороченное описание в одну строку (ограничиваем 28 символами)
    char short_desc[29];
    strncpy(short_desc, description, 28);
    short_desc[28] = '\0';
    tft_.setTextColor(0xFDA0, 0x4000); // светло-оранжевый
    tft_.drawString(short_desc, 8, 34, 2); // font 2 ≈ 16 px высота

    // Восстанавливаем разделитель секции температуры
    tft_.drawFastHLine(0, 59, 240, 0x5AEB);

    strncpy(drawn_alert_code_, code, sizeof(drawn_alert_code_) - 1);
    drawn_alert_code_[sizeof(drawn_alert_code_) - 1] = '\0';
    alert_visible_ = true;
}

void DisplayManager::clear_alert()
{
    if (!alert_visible_) return;

    // Восстанавливаем статический заголовок
    draw_header_();

    drawn_alert_code_[0] = '\0';
    alert_visible_       = false;
}

// ─────────────────────────────────────────────────────────────────────────────

uint16_t DisplayManager::get_temperature_color(float value, float min_temp,
                                               float target_temp, float max_temp)
{
    // ЖЁСТКИЙ СТОПОР: температура равна или выше максимума — чистый КРАСНЫЙ (RGB565)
    if (value >= max_temp) {
        return 0xF800;
    }

    // ЖЁСТКИЙ СТОПОР: температура ниже или равна минимуму — чистый СИНИЙ (RGB565)
    if (value <= min_temp) {
        return 0x001F;
    }

    // Защита от деления на ноль при некорректном конфиге
    if (target_temp <= min_temp || max_temp <= target_temp) {
        return 0xFFFF; // белый — сигнал ошибки конфига
    }

    float factor = 0.0f;
    float hue    = 120.0f; // по умолчанию чистый зелёный (120° в HSV)

    if (value < target_temp) {
        // Отрезок 1: от Синего (240°) до Зелёного (120°)
        factor = (value - min_temp) / (target_temp - min_temp);
        hue    = 240.0f - (factor * 120.0f);
    } else {
        // Отрезок 2: от Зелёного (120°) до Красного (0°)
        factor = (value - target_temp) / (max_temp - target_temp);
        hue    = 120.0f - (factor * 120.0f);
    }

    // Быстрая конвертация Hue → RGB565 для дисплея
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
    const float green_start = config.get("rpm", "green_start"); // нач. зелёной зоны
    const float green_end   = config.get("rpm", "green_end");   // конец зелёной зоны
    const float red_start   = config.get("rpm", "red_start");   // начало красной зоны

    // Ниже начала зелёной зоны — синий
    if (rpm <= green_start) {
        if (rpm <= 750.0f) return 0x001F; // чистый синий

        // Плавный переход: синий → зелёный (750..green_start)
        // hue: 240° → 120°, h: 4.0 → 2.0
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

    // Чистый зелёный (green_start..green_end)
    if (rpm <= green_end) return 0x07E0;

    // Выше конца красной зоны — чистый красный
    if (rpm >= red_start) return 0xF800;

    // Плавный переход: зелёный → жёлтый → красный (green_end..red_start)
    float t   = (rpm - green_end) / (red_start - green_end);
    float hue = 120.0f - t * 120.0f; // 120° → 0°
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
    const float blue_max  = config.get("boost", "blue_max");   // ≤ этого — синий
    const float green_min = config.get("boost", "green_min");  // ≥ этого — зелёный

    if (boost <= blue_max)  return 0x001F; // чистый синий
    if (boost >= green_min) return 0x07E0; // чистый зелёный

    // Плавный переход: синий→жёлтый→зелёный (blue_max..green_min)
    // hue: 240° → 120° через 60° (жёлтый посередине диапазона hue)
    float t   = (boost - blue_max) / (green_min - blue_max); // 0..1
    float hue = 240.0f - t * 120.0f;                         // 240..120
    float h   = hue / 60.0f;                                  // 4.0..2.0
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
    // Обороты нулевые или нет данных — синий
    if (rpm == 0.0f || poll_time == 0.0f) return 0x001F;

    const float green_max = config.get("poll_time", "green_max"); // ≤ этого — зелёный
    const float red_min   = config.get("poll_time", "red_min");   // ≥ этого — красный

    // Быстрый ответ — чистый зелёный
    if (poll_time <= green_max) return 0x07E0;

    // Медленный ответ — чистый красный
    if (poll_time >= red_min) return 0xF800;

    // Плавный переход зелёный → жёлтый → красный (green_max..red_min)
    // hue: 120° → 0° (зелёный → красный через жёлтый)
    float t   = (poll_time - green_max) / (red_min - green_max); // 0..1
    float hue = 120.0f - t * 120.0f;                             // 120°..0°
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
    // Нет данных (CAN устарел → 0.0) — синий
    if (voltage == 0.0f) return 0x001F;

    const float red_low   = config.get("battery", "red_low");   // < этого — красный
    const float green_min = config.get("battery", "green_min"); // начало зелёной зоны
    const float green_max = config.get("battery", "green_max"); // конец зелёной зоны
    const float red_high  = config.get("battery", "red_high");  // > этого — красный

    // Жёсткие стопоры: ниже red_low или выше red_high — чистый красный
    if (voltage < red_low)  return 0xF800;
    if (voltage > red_high) return 0xF800;

    // Зелёная зона: green_min..green_max
    if (voltage >= green_min && voltage <= green_max) return 0x07E0;

    // Переход красный→жёлтый→зелёный: red_low..green_min (hue: 0°→60°→120°)
    if (voltage < green_min) {
        float t   = (voltage - red_low) / (green_min - red_low); // 0..1
        float hue = t * 120.0f;                                   // 0°..120°
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

    // Переход зелёный→жёлтый→красный: green_max..red_high (hue: 120°→60°→0°)
    {
        float t   = (voltage - green_max) / (red_high - green_max); // 0..1
        float hue = 120.0f - t * 120.0f;                            // 120°..0°
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
    // Нет данных (CAN устарел → 0.0) — синий
    if (pressure == 0.0f) return 0x001F;

    float threshold    = config.get("oil_pressure", "rpm_threshold");
    float min_pressure = (rpm < threshold)
        ? config.get("oil_pressure", "min_low")
        : config.get("oil_pressure", "min_high");

    return (pressure < min_pressure) ? 0xF800 : 0x07E0; // красный или зелёный
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

    // Обороты двигателя: 750..6000 — плавный цвет синий→зелёный→жёлтый→красный
    // setTextPadding затирает 4-й символ фоном при 3-значном числе
    tft_.setTextColor(get_rpm_color(rpm), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("6000", 4));
    snprintf(buf, sizeof(buf), "%.0f", rpm);
    tft_.drawString(buf, 12, 146, 4);
    tft_.setTextPadding(0);

    // Давление масла: цвет зависит от оборотов (красный если ниже нормы)
    tft_.setTextColor(get_oil_pressure_color(oil_pressure, rpm), TFT_BLACK);
    snprintf(buf, sizeof(buf), "%.2f", oil_pressure);
    tft_.drawString(buf, 95, 146, 4);

    // Давление наддува (Вольты): 0.50..4.50 В
    // setTextPadding затирает остаток при переходе от 5 к 4 символам
    tft_.setTextColor(get_boost_color(boost), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("-0.50", 4));
    snprintf(buf, sizeof(buf), "%.2f", boost);
    tft_.drawString(buf, 175, 146, 4);
    tft_.setTextPadding(0);

    // Вольтаж бортовой сети: 11.0..15.0 Вольт
    tft_.setTextColor(get_battery_color(battery_voltage), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("14.99", 4));
    snprintf(buf, sizeof(buf), "%.2f", battery_voltage);
    tft_.drawString(buf, 7, 205, 4);
    tft_.setTextPadding(0);

    // Время опроса RPM: зелёный(≤0.2с) → красный(≥0.5с), синий если rpm=0
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
