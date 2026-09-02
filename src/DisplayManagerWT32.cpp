#include "DisplayManagerWT32.h"

#include <math.h>

#include "ConfigManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Константы разметки дисплея 480×320 (альбомная ориентация)
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int16_t SCREEN_WIDTH = 480;
static constexpr int16_t HEADER_HEIGHT = 49;
static constexpr int16_t TILE_X[] = {6, 164, 322};
static constexpr int16_t TILE_Y[] = {52, 132, 212};
static constexpr int16_t TILE_WIDTH = 152;
static constexpr int16_t TILE_HEIGHT = 76;
static constexpr int16_t TILE_RADIUS = 7;
static constexpr int16_t TILE_LABEL_OFFSET_Y = 6;
static constexpr int16_t TILE_VALUE_OFFSET_Y = 34;
static constexpr int16_t TILE_STATUS_WIDTH = 5;
static constexpr int16_t TILE_STATUS_INSET = 3;
static constexpr int16_t TILE_STATUS_RADIUS = 3;
static constexpr int16_t LABEL_SPRITE_WIDTH = 280;
static constexpr int16_t LABEL_SPRITE_HEIGHT = 44;
static constexpr float LABEL_SPRITE_ZOOM = 0.5f;
static constexpr int16_t SMOOTH_SPRITE_WIDTH = 460;
static constexpr int16_t SMOOTH_SPRITE_HEIGHT = 48;
static constexpr int16_t VALUE_SPRITE_WIDTH = 132;
static constexpr float VALUE_TEXT_ZOOM = 1.0f;
static constexpr int16_t VER_Y = 306;

static constexpr uint16_t COLOR_CARD = TFT_BLACK;
static constexpr uint16_t COLOR_BORDER = 0x2945;
static constexpr uint16_t COLOR_ACCENT = 0xCE70;
static constexpr uint16_t COLOR_MUTED = 0x8410;
static constexpr uint16_t COLOR_NO_DATA = 0x52AA;
static constexpr uint16_t COLOR_TRANSPARENT = 0xF81F;

// ─────────────────────────────────────────────────────────────────────────────

Wt32Display::Wt32Display()
{
    {
        auto config = bus_.config();
        config.freq_write = 20000000;
        config.pin_wr = 47;
        config.pin_rd = -1;
        config.pin_rs = 0;
        config.pin_d0 = 9;
        config.pin_d1 = 46;
        config.pin_d2 = 3;
        config.pin_d3 = 8;
        config.pin_d4 = 18;
        config.pin_d5 = 17;
        config.pin_d6 = 16;
        config.pin_d7 = 15;
        bus_.config(config);
        panel_.setBus(&bus_);
    }

    {
        auto config = panel_.config();
        config.pin_cs = -1;
        config.pin_rst = 4;
        config.pin_busy = -1;
        config.memory_width = 320;
        config.memory_height = 480;
        config.panel_width = 320;
        config.panel_height = 480;
        config.offset_x = 0;
        config.offset_y = 0;
        config.offset_rotation = 0;
        config.readable = false;
        config.invert = true;
        config.rgb_order = false;
        config.dlen_16bit = false;
        config.bus_shared = false;
        panel_.config(config);
    }

    setPanel(&panel_);
}

int32_t Wt32Display::text_width(const char *text, uint8_t font)
{
    return textWidth(text, lgfx::fontdata[font]);
}

DisplayManagerWT32::DisplayManagerWT32()
    : alert_visible_(false), alert_indicator_(false), brightness_duty_(0)
{
    version_buf_[0]      = '\0';
    drawn_alert_code_[0] = '\0';
}

// Пин подсветки дисплея WT32-SC01 Plus (GPIO45)
static constexpr uint8_t WT32_BL_PIN = 45;
static constexpr uint8_t WT32_BL_LEDC_CHANNEL = 2;
static constexpr uint32_t WT32_BL_LEDC_FREQ_HZ = 5000;
static constexpr uint8_t WT32_BL_LEDC_BITS = 8;
static constexpr uint8_t WT32_BL_MAX_DUTY = 255;
static constexpr float BRIGHTNESS_MIN_PERCENT = 10.0f;
static constexpr float BRIGHTNESS_MAX_PERCENT = 100.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Внутренний метод: рисует все статические заголовки и лейблы
// Вызывается при init() и при clear_alert() для восстановления интерфейса

void DisplayManagerWT32::draw_static_()
{
    draw_header_();

    draw_metric_tile_(0, "RAD-ANT", "C");
    draw_metric_tile_(1, "ENG-ANT", "C");
    draw_metric_tile_(2, "ENG-OIL", "C");
    draw_metric_tile_(3, "ENG-RPM", "RPM");
    draw_metric_tile_(4, "OIL-PR-V", "V");
    draw_metric_tile_(5, "TURBO-V", "V");
    draw_metric_tile_(6, "BATTERY-V", "V");
    draw_metric_tile_(7, "RPM-POLL", "S");
    draw_metric_tile_(8, "CVT-FLD", "C");

    // Версия прошивки внизу
    if (version_buf_[0]) {
        if (!draw_smooth_text_(version_buf_, SCREEN_WIDTH / 2, VER_Y + 4,
                               COLOR_ACCENT, 0.3f, SMOOTH_SPRITE_WIDTH)) {
            tft_.setTextColor(COLOR_ACCENT, TFT_BLACK);
            tft_.drawCentreString(version_buf_, SCREEN_WIDTH / 2, VER_Y, lgfx::fontdata[1]);
        }
    }
}

// Внутренний метод: перерисовывает только заголовок
// Используется для частичного восстановления интерфейса

void DisplayManagerWT32::draw_header_()
{
    tft_.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_BLACK);

    lgfx::LGFX_Sprite header_sprite(&tft_);
    header_sprite.setColorDepth(16);
    if (header_sprite.createSprite(700, SMOOTH_SPRITE_HEIGHT)) {
        header_sprite.fillSprite(TFT_BLACK);
        header_sprite.setFont(&lgfx::fonts::FreeSansBold18pt7b);
        header_sprite.setTextDatum(lgfx::textdatum_t::middle_center);
        header_sprite.setTextColor(TFT_WHITE, TFT_BLACK);
        header_sprite.drawString("INFINITI QX50 J55", 220, SMOOTH_SPRITE_HEIGHT / 2);
        header_sprite.setTextColor(COLOR_ACCENT, TFT_BLACK);
        header_sprite.drawString("MONITORING", 555, SMOOTH_SPRITE_HEIGHT / 2);
        header_sprite.setPivot(350, SMOOTH_SPRITE_HEIGHT / 2);
        header_sprite.pushRotateZoomWithAA(&tft_, SCREEN_WIDTH / 2, 24, 0.0f, 0.65f, 0.65f);
        return;
    }

    if (!draw_smooth_text_("INFINITI QX50 J55", 155, 24, TFT_WHITE, 0.55f,
                           SMOOTH_SPRITE_WIDTH)) {
        tft_.setTextColor(TFT_WHITE, TFT_BLACK);
        tft_.drawCentreString("INFINITI QX50 J55", 155, 11, lgfx::fontdata[4]);
    }
    if (!draw_smooth_text_("MONITORING", 370, 24, COLOR_ACCENT, 0.55f, 300)) {
        tft_.setTextColor(COLOR_ACCENT, TFT_BLACK);
        tft_.drawCentreString("MONITORING", 370, 11, lgfx::fontdata[4]);
    }
}

bool DisplayManagerWT32::draw_smooth_text_(const char *text, int16_t center_x, int16_t center_y,
                                            uint16_t color, float zoom, int16_t sprite_width)
{
    lgfx::LGFX_Sprite sprite(&tft_);
    sprite.setColorDepth(16);
    if (!sprite.createSprite(sprite_width, SMOOTH_SPRITE_HEIGHT)) {
        return false;
    }

    sprite.fillSprite(COLOR_TRANSPARENT);
    sprite.setFont(&lgfx::fonts::FreeSansBold18pt7b);
    sprite.setTextColor(color, COLOR_TRANSPARENT);
    sprite.setTextDatum(lgfx::textdatum_t::middle_center);
    sprite.drawString(text, sprite_width / 2, SMOOTH_SPRITE_HEIGHT / 2);
    sprite.setPivot(sprite_width / 2, SMOOTH_SPRITE_HEIGHT / 2);
    sprite.pushRotateZoomWithAA(&tft_, center_x, center_y, 0.0f, zoom, zoom, COLOR_TRANSPARENT);
    return true;
}

bool DisplayManagerWT32::draw_smooth_value_(const char *text, int16_t center_x, int16_t center_y,
                                             uint16_t color)
{
    lgfx::LGFX_Sprite sprite(&tft_);
    sprite.setColorDepth(16);
    if (!sprite.createSprite(VALUE_SPRITE_WIDTH, SMOOTH_SPRITE_HEIGHT)) {
        return false;
    }

    sprite.fillSprite(COLOR_CARD);
    sprite.setFont(&lgfx::fonts::FreeSansBold18pt7b);
    sprite.setTextColor(color, COLOR_CARD);
    sprite.setTextDatum(lgfx::textdatum_t::middle_center);
    sprite.drawString(text, VALUE_SPRITE_WIDTH / 2, SMOOTH_SPRITE_HEIGHT / 2);
    sprite.setPivot(VALUE_SPRITE_WIDTH / 2, SMOOTH_SPRITE_HEIGHT / 2);
    sprite.pushRotateZoomWithAA(&tft_, center_x, center_y, 0.0f,
                                VALUE_TEXT_ZOOM, VALUE_TEXT_ZOOM);
    return true;
}

void DisplayManagerWT32::draw_metric_tile_(uint8_t index, const char *label, const char *unit)
{
    const int16_t x = TILE_X[index % 3];
    const int16_t y = TILE_Y[index / 3];
    const int16_t center_x = x + (TILE_WIDTH / 2);
    char unit_buf[12];
    snprintf(unit_buf, sizeof(unit_buf), ", %s", unit);

    tft_.fillRoundRect(x, y, TILE_WIDTH, TILE_HEIGHT, TILE_RADIUS, COLOR_CARD);
    tft_.drawRoundRect(x, y, TILE_WIDTH, TILE_HEIGHT, TILE_RADIUS, COLOR_BORDER);

    tft_.fillRoundRect(x + 1, y + TILE_STATUS_INSET, TILE_STATUS_WIDTH,
                       TILE_HEIGHT - (TILE_STATUS_INSET * 2), TILE_STATUS_RADIUS, COLOR_NO_DATA);

    lgfx::LGFX_Sprite label_sprite(&tft_);
    label_sprite.setColorDepth(16);
    if (label_sprite.createSprite(LABEL_SPRITE_WIDTH, LABEL_SPRITE_HEIGHT)) {
        label_sprite.fillSprite(COLOR_TRANSPARENT);
        label_sprite.setFont(&lgfx::fonts::FreeSansBold18pt7b);
        const int32_t label_width = label_sprite.textWidth(label);
        const int32_t unit_width = label_sprite.textWidth(unit_buf);
        const int32_t text_x = (LABEL_SPRITE_WIDTH - label_width - unit_width) / 2;
        label_sprite.setTextDatum(lgfx::textdatum_t::middle_left);
        label_sprite.setTextColor(COLOR_ACCENT, COLOR_TRANSPARENT);
        label_sprite.drawString(label, text_x, LABEL_SPRITE_HEIGHT / 2);
        label_sprite.setTextColor(COLOR_MUTED, COLOR_TRANSPARENT);
        label_sprite.drawString(unit_buf, text_x + label_width, LABEL_SPRITE_HEIGHT / 2);
        label_sprite.setPivot(LABEL_SPRITE_WIDTH / 2, LABEL_SPRITE_HEIGHT / 2);
        label_sprite.pushRotateZoomWithAA(&tft_, center_x, y + TILE_LABEL_OFFSET_Y + 7,
                                          0.0f, LABEL_SPRITE_ZOOM, LABEL_SPRITE_ZOOM,
                                          COLOR_TRANSPARENT);
    } else {
        const int32_t label_width = tft_.text_width(label, 2);
        const int32_t unit_width = tft_.text_width(unit_buf, 2);
        const int32_t text_x = center_x - ((label_width + unit_width) / 2);
        tft_.setTextColor(COLOR_ACCENT, COLOR_CARD);
        tft_.drawString(label, text_x, y + TILE_LABEL_OFFSET_Y, lgfx::fontdata[2]);
        tft_.setTextColor(COLOR_MUTED, COLOR_CARD);
        tft_.drawString(unit_buf, text_x + label_width, y + TILE_LABEL_OFFSET_Y,
                        lgfx::fontdata[2]);
    }
    label_sprite.deleteSprite();

    if (!draw_smooth_value_("--", center_x, y + TILE_VALUE_OFFSET_Y + 16, COLOR_NO_DATA)) {
        tft_.setTextColor(COLOR_NO_DATA, COLOR_CARD);
        tft_.setTextSize(1.35f);
        tft_.drawCentreString("--", center_x, y + TILE_VALUE_OFFSET_Y, lgfx::fontdata[4]);
        tft_.setTextSize(1.0f);
    }
}

void DisplayManagerWT32::draw_metric_value_(uint8_t index, const char *value, uint16_t color)
{
    const int16_t x = TILE_X[index % 3];
    const int16_t y = TILE_Y[index / 3];
    const int16_t center_x = x + (TILE_WIDTH / 2);

    tft_.fillRoundRect(x + 1, y + TILE_STATUS_INSET, TILE_STATUS_WIDTH,
                       TILE_HEIGHT - (TILE_STATUS_INSET * 2), TILE_STATUS_RADIUS, color);
    if (!draw_smooth_value_(value, center_x, y + TILE_VALUE_OFFSET_Y + 16, color)) {
        tft_.setTextColor(color, COLOR_CARD);
        tft_.setTextSize(1.35f);
        tft_.setTextPadding(TILE_WIDTH - 12);
        tft_.drawCentreString(value, center_x, y + TILE_VALUE_OFFSET_Y, lgfx::fontdata[4]);
        tft_.setTextPadding(0);
        tft_.setTextSize(1.0f);
    }
}

void DisplayManagerWT32::init(const char *version)
{
    ledcSetup(WT32_BL_LEDC_CHANNEL, WT32_BL_LEDC_FREQ_HZ, WT32_BL_LEDC_BITS);
    ledcAttachPin(WT32_BL_PIN, WT32_BL_LEDC_CHANNEL);
    update_brightness_();

    tft_.init();
    tft_.setRotation(1); // альбомная ориентация 480×320
    tft_.fillScreen(TFT_BLACK);

    if (version) {
        snprintf(version_buf_, sizeof(version_buf_), "Version %s", version);
    }

    draw_static_();
}

void DisplayManagerWT32::update_brightness_()
{
    float brightness_percent = config.get("system", "brightness_percent");
    if (brightness_percent < BRIGHTNESS_MIN_PERCENT) {
        brightness_percent = BRIGHTNESS_MIN_PERCENT;
    } else if (brightness_percent > BRIGHTNESS_MAX_PERCENT) {
        brightness_percent = BRIGHTNESS_MAX_PERCENT;
    }

    const uint8_t duty = static_cast<uint8_t>(
        ((brightness_percent / 100.0f) * WT32_BL_MAX_DUTY) + 0.5f);

    if (duty != brightness_duty_) {
        brightness_duty_ = duty;
        ledcWrite(WT32_BL_LEDC_CHANNEL, brightness_duty_);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Алерт-оверлей: занимает ВЕСЬ экран, метрики в это время не обновляются

void DisplayManagerWT32::show_alert(const char *code, const char *display_name)
{
    // Перерисовываем только если код изменился
    if (alert_visible_ && strncmp(drawn_alert_code_, code, sizeof(drawn_alert_code_)) == 0) {
        return;
    }

    // Очищаем весь экран
    tft_.fillScreen(TFT_BLACK);

    // Код алерта вверху
    if (!draw_smooth_text_(code, SCREEN_WIDTH / 2, 32, COLOR_ACCENT, 0.8f,
                           SMOOTH_SPRITE_WIDTH)) {
        tft_.setTextColor(COLOR_ACCENT, TFT_BLACK);
        tft_.drawCentreString(code, SCREEN_WIDTH / 2, 18, lgfx::fontdata[4]);
    }

    // Разделительная линия
    tft_.drawFastHLine(10, 58, SCREEN_WIDTH - 20, COLOR_ACCENT);

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
        memcpy(targets[i], p, static_cast<size_t>(copy_len));
        targets[i][copy_len] = '\0';
        p = nl ? nl + 1 : nullptr;
    }

    // Позиционирование рассчитано для области ниже разделителя на экране 480×320
    static constexpr int16_t LINE_H = 26;   // высота шрифта 4
    static constexpr int16_t LINE_GAP = 18; // отступ между строками
    static constexpr int16_t Y_START = 132; // верх первой строки

    // Строка 1: красная
    if (!draw_smooth_text_(line1, SCREEN_WIDTH / 2, Y_START + (LINE_H / 2), 0xF800,
                           VALUE_TEXT_ZOOM, SMOOTH_SPRITE_WIDTH)) {
        tft_.setTextColor(0xF800, TFT_BLACK);
        tft_.drawCentreString(line1, SCREEN_WIDTH / 2, Y_START, lgfx::fontdata[4]);
    }

    // Строка 2: белая
    if (!draw_smooth_text_(line2, SCREEN_WIDTH / 2, Y_START + LINE_H + LINE_GAP + (LINE_H / 2),
                           TFT_WHITE, VALUE_TEXT_ZOOM, SMOOTH_SPRITE_WIDTH)) {
        tft_.setTextColor(TFT_WHITE, TFT_BLACK);
        tft_.drawCentreString(line2, SCREEN_WIDTH / 2, Y_START + LINE_H + LINE_GAP,
                              lgfx::fontdata[4]);
    }

    // Строка 3: красная
    if (!draw_smooth_text_(line3, SCREEN_WIDTH / 2,
                           Y_START + ((LINE_H + LINE_GAP) * 2) + (LINE_H / 2),
                           0xF800, VALUE_TEXT_ZOOM, SMOOTH_SPRITE_WIDTH)) {
        tft_.setTextColor(0xF800, TFT_BLACK);
        tft_.drawCentreString(line3, SCREEN_WIDTH / 2, Y_START + (LINE_H + LINE_GAP) * 2,
                              lgfx::fontdata[4]);
    }

    strncpy(drawn_alert_code_, code, sizeof(drawn_alert_code_) - 1);
    drawn_alert_code_[sizeof(drawn_alert_code_) - 1] = '\0';
    alert_visible_ = true;
}

void DisplayManagerWT32::clear_alert()
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

void DisplayManagerWT32::update_alert_indicator(bool has_alerts)
{
    // Перерисовываем только при изменении состояния
    if (has_alerts == alert_indicator_) return;
    alert_indicator_ = has_alerts;

    // Кружок слева от заголовка
    static constexpr int16_t IND_X = 20;
    static constexpr int16_t IND_Y = 24;
    static constexpr int16_t IND_R = 8;

    if (has_alerts) {
        tft_.fillCircle(IND_X, IND_Y, IND_R, 0xF800); // красный
    } else {
        tft_.fillCircle(IND_X, IND_Y, IND_R, TFT_BLACK); // стираем
    }
}

// ─────────────────────────────────────────────────────────────────────────────

uint16_t DisplayManagerWT32::get_temperature_color(float value, float min_temp,
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

uint16_t DisplayManagerWT32::get_rpm_color(float rpm)
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

uint16_t DisplayManagerWT32::get_boost_color(float boost)
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

uint16_t DisplayManagerWT32::get_poll_time_color(float poll_time, float rpm)
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

uint16_t DisplayManagerWT32::get_battery_color(float voltage)
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

uint16_t DisplayManagerWT32::get_oil_pressure_color(float pressure, float rpm)
{
    if (pressure == 0.0f) return 0x001F;

    float threshold    = config.get("oil_pressure", "rpm_threshold");
    float min_pressure = (rpm < threshold)
        ? config.get("oil_pressure", "min_low")
        : config.get("oil_pressure", "min_high");

    return (pressure < min_pressure) ? 0xF800 : 0x07E0;
}

void DisplayManagerWT32::update_metrics(float coolant, float oil, float coolant_r,
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
    snprintf(buf, sizeof(buf), "%.0f", coolant_r);
    draw_metric_value_(0, buf, radiator_color);

    // Антифриз ДВС
    uint16_t coolant_color = get_temperature_color(coolant,
        config.get("coolant", "min"),
        config.get("coolant", "target"),
        config.get("coolant", "max"));
    snprintf(buf, sizeof(buf), "%.0f", coolant);
    draw_metric_value_(1, buf, coolant_color);

    // Моторное масло
    uint16_t oil_color = get_temperature_color(oil,
        config.get("oil", "min"),
        config.get("oil", "target"),
        config.get("oil", "max"));
    snprintf(buf, sizeof(buf), "%.0f", oil);
    draw_metric_value_(2, buf, oil_color);

    // Обороты двигателя
    snprintf(buf, sizeof(buf), "%.0f", rpm);
    draw_metric_value_(3, buf, get_rpm_color(rpm));

    // Напряжение датчика давления масла
    snprintf(buf, sizeof(buf), "%.2f", oil_pressure);
    draw_metric_value_(4, buf, get_oil_pressure_color(oil_pressure, rpm));

    // Давление наддува
    snprintf(buf, sizeof(buf), "%.2f", boost);
    draw_metric_value_(5, buf, get_boost_color(boost));

    // Вольтаж бортовой сети
    snprintf(buf, sizeof(buf), "%.2f", battery_voltage);
    draw_metric_value_(6, buf, get_battery_color(battery_voltage));

    // Период обновления RPM
    if (rpm == 0.0f || poll_time == 0.0f) {
        snprintf(buf, sizeof(buf), "0");
    } else {
        snprintf(buf, sizeof(buf), "%.2f", poll_time);
    }
    draw_metric_value_(7, buf, get_poll_time_color(poll_time, rpm));

    // Масло коробки
    uint16_t transmission_color = get_temperature_color(transmission,
        config.get("transmission", "min"),
        config.get("transmission", "target"),
        config.get("transmission", "max"));
    snprintf(buf, sizeof(buf), "%.0f", transmission);
    draw_metric_value_(8, buf, transmission_color);
}
