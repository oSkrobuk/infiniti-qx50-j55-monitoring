#include "MetricColors.h"

#include <math.h>

#include "ConfigManager.h"

uint16_t metric_temperature_color(float value, float min_temp,
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

uint16_t metric_rpm_color(float rpm)
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

uint16_t metric_boost_color(float boost)
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

uint16_t metric_poll_time_color(float poll_time, float rpm)
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

uint16_t metric_battery_color(float voltage)
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

uint16_t metric_oil_pressure_color(float pressure, float rpm)
{
    if (pressure == 0.0f) return 0x001F;

    float threshold    = config.get("oil_pressure", "rpm_threshold");
    float min_pressure = (rpm < threshold)
        ? config.get("oil_pressure", "min_low")
        : config.get("oil_pressure", "min_high");

    return (pressure < min_pressure) ? 0xF800 : 0x07E0;
}
