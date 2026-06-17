#include "DisplayManager.h"

#include "ConfigManager.h"

DisplayManager::DisplayManager() : tft_(TFT_eSPI()) {}

void DisplayManager::init(const char *version)
{
    tft_.init();
    tft_.setRotation(0);
    tft_.fillScreen(TFT_BLACK);

    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString("INFINITI QX50 J55", 30, 5, 4);
    tft_.setTextColor(0xCE70, TFT_BLACK);
    tft_.drawString("MONITORING", 75, 28, 4);

    // Температура
    tft_.drawFastHLine(0, 59, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" TEMPERATURE, C ", 120, 51, 2);

    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawString("R-COOL", 10,  69, 2);
    tft_.drawString("E-COOL", 70,  69, 2);
    tft_.drawString("E-OIL",  130, 69, 2);
    tft_.drawString("T-OIL",  190, 69, 2);

    // Двигатель
    tft_.drawFastHLine(0, 118, 240, 0x5AEB);
    tft_.setTextColor(0x5AEB, TFT_BLACK);
    tft_.drawCentreString(" ENGINE ", 120, 110, 2);

    tft_.setTextColor(0x9CD3, TFT_BLACK);
    tft_.drawString("RPM",   10,  128, 2);
    tft_.drawString("EOP",   95,  128, 2);
    tft_.drawString("BOOST", 170, 128, 2);

    // Версия прошивки — мелким шрифтом внизу экрана
    if (version) {
        char ver_buf[32];
        snprintf(ver_buf, sizeof(ver_buf), "Version %s", version);
        tft_.setTextColor(0xCE70, TFT_BLACK); // тёмно-серый
        tft_.drawCentreString(ver_buf, 120, 224, 2);
    }
}

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

uint16_t DisplayManager::get_oil_pressure_color(float pressure, float rpm)
{
    float threshold    = config.get("oil_pressure", "rpm_threshold");
    float min_pressure = (rpm < threshold)
        ? config.get("oil_pressure", "min_low")
        : config.get("oil_pressure", "min_high");

    return (pressure < min_pressure) ? 0xF800 : 0x07E0; // красный или зелёный
}

void DisplayManager::update_metrics(float coolant, float oil, float coolant_r,
                                    float transmission, float rpm,
                                    float oil_pressure, float boost)
{
    char buf[12];

    // Антифриз радиатора
    uint16_t radiator_color = get_temperature_color(coolant_r,
        config.get("radiator", "min"),
        config.get("radiator", "target"),
        config.get("radiator", "max"));
    tft_.setTextColor(radiator_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", coolant_r);
    tft_.drawString(buf, 10, 87, 4);

    // Антифриз ДВС
    uint16_t coolant_color = get_temperature_color(coolant,
        config.get("coolant", "min"),
        config.get("coolant", "target"),
        config.get("coolant", "max"));
    tft_.setTextColor(coolant_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", coolant);
    tft_.drawString(buf, 70, 87, 4);

    // Моторное масло
    uint16_t oil_color = get_temperature_color(oil,
        config.get("oil", "min"),
        config.get("oil", "target"),
        config.get("oil", "max"));
    tft_.setTextColor(oil_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", oil);
    tft_.drawString(buf, 130, 87, 4);

    // Масло коробки
    uint16_t transmission_color = get_temperature_color(transmission,
        config.get("transmission", "min"),
        config.get("transmission", "target"),
        config.get("transmission", "max"));
    tft_.setTextColor(transmission_color, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%-5.0f", transmission);
    tft_.drawString(buf, 190, 87, 4);

    // Обороты двигателя: 750..6000 — плавный цвет синий→зелёный→жёлтый→красный
    // setTextPadding затирает 4-й символ фоном при 3-значном числе
    tft_.setTextColor(get_rpm_color(rpm), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("6000", 4));
    snprintf(buf, sizeof(buf), "%.0f", rpm);
    tft_.drawString(buf, 10, 146, 4);
    tft_.setTextPadding(0);

    // Давление масла: цвет зависит от оборотов (красный если ниже нормы)
    tft_.setTextColor(get_oil_pressure_color(oil_pressure, rpm), TFT_BLACK);
    snprintf(buf, sizeof(buf), "%.2f", oil_pressure);
    tft_.drawString(buf, 95, 146, 4);

    // Давление наддува: -0.50..1.20 бар, 2 знака (5 символов с минусом, 4 без)
    // setTextPadding затирает остаток при переходе от 5 к 4 символам
    tft_.setTextColor(get_boost_color(boost), TFT_BLACK);
    tft_.setTextPadding(tft_.textWidth("-0.50", 4));
    snprintf(buf, sizeof(buf), "%.2f", boost);
    tft_.drawString(buf, 170, 146, 4);
    tft_.setTextPadding(0);
}
