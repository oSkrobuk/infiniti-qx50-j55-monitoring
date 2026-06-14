#include "DisplayManager.h"
#include "ConfigManager.h"

DisplayManager::DisplayManager() : tft(TFT_eSPI()) {}

void DisplayManager::init()
{
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // tft.drawRect(0, 0, 240, 240, TFT_DARKGREY);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("INFINITI QX50 J55", 30, 5, 4);
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.drawString("MONITORING", 75, 28, 4);

    tft.drawFastHLine(5, 59, 240, TFT_SILVER);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("Temperature, C", 5, 51, 2);

    tft.drawString("E-Oil", 15, 69, 2);
    tft.drawString("E-Cool", 75, 69, 2);
    tft.drawString("R-Cool", 135, 69, 2);
}

uint16_t DisplayManager::getTemperatureColor(float value, float minTemp, float targetTemp, float maxTemp)
{
    float factor = 0.0f;
    float hue = 120.0f; // По умолчанию чистый зеленый (120 градусов в HSV)

    if (value <= minTemp)
    {
        hue = 240.0f; // Синий для совсем холодного
    }
    else if (value >= maxTemp)
    {
        hue = 0.0f; // Красный для перегрева
    }
    else if (value < targetTemp)
    {
        // Отрезок 1: от Синего (240) до Зеленого (120)
        factor = (value - minTemp) / (targetTemp - minTemp);
        hue = 240.0f - (factor * 120.0f);
    }
    else
    {
        // Отрезок 2: от Зеленого (120) до Красного (0)
        factor = (value - targetTemp) / (maxTemp - targetTemp);
        hue = 120.0f - (factor * 120.0f);
    }

    // Быстрая конвертация Hue в RGB565 (на полной скорости с флагом -O3)
    float h = hue / 60.0f;
    float x = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
    float r = 0, g = 0, b = 0;

    if (h >= 0 && h < 1)
    {
        r = 1;
        g = x;
        b = 0;
    }
    else if (h >= 1 && h < 2)
    {
        r = x;
        g = 1;
        b = 0;
    }
    else if (h >= 2 && h < 3)
    {
        r = 0;
        g = 1;
        b = x;
    }
    else
    {
        r = 0;
        g = x;
        b = 1;
    } // Для диапазона 0-240 градусов другие условия не нужны

    uint16_t r565 = (((uint16_t)(r * 31)) << 11) | (((uint16_t)(g * 63)) << 5) | ((uint16_t)(b * 31));
    return r565;
}

void DisplayManager::updateMetrics(float coolant, float oil, float coolantR)
{
    char buf[12];

    // 1. Моторное масло: читаем пороги из config.oil
    uint16_t oilColor = getTemperatureColor(oil, config.oil.min, config.oil.target, config.oil.max);
    tft.setTextColor(oilColor, TFT_BLACK);
    sprintf(buf, "%-5.0f", oil);
    tft.drawString(buf, 15, 87, 4);

    // 2. Антифриз ДВС: читаем пороги из config.coolant
    uint16_t coolantColor = getTemperatureColor(coolant, config.coolant.min, config.coolant.target, config.coolant.max);
    tft.setTextColor(coolantColor, TFT_BLACK);
    sprintf(buf, "%-5.0f", coolant);
    tft.drawString(buf, 75, 87, 4);

    // 3. Антифриз радиатора: читаем пороги из config.radiator
    uint16_t radiatorColor = getTemperatureColor(coolantR, config.radiator.min, config.radiator.target, config.radiator.max);
    tft.setTextColor(radiatorColor, TFT_BLACK);
    sprintf(buf, "%-5.0f", coolantR);
    tft.drawString(buf, 135, 87, 4);
}
