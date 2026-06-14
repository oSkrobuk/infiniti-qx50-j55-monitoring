#include "DisplayManager.h"
#include "ConfigManager.h"

DisplayManager::DisplayManager() : tft(TFT_eSPI()) {}

void DisplayManager::init()
{
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("INFINITI QX50 J55", 30, 5, 4);
    tft.setTextColor(0xCE70, TFT_BLACK);
    tft.drawString("MONITORING", 75, 28, 4);

    tft.drawFastHLine(115, 59, 240, 0x5AEB);
    tft.setTextColor(0x5AEB, TFT_BLACK);
    tft.drawString("TEMPERATURE, C", 5, 51, 2);

    tft.setTextColor(0x9CD3, TFT_BLACK);
    tft.drawString("E-OIL", 10, 69, 2);
    tft.drawString("E-COOL", 70, 69, 2);
    tft.drawString("R-COOL", 130, 69, 2);
    tft.drawString("T-OIL", 190, 69, 2);
}

uint16_t DisplayManager::getTemperatureColor(float value, float minTemp, float targetTemp, float maxTemp)
{
    // ЖЕСТКИЙ СТОПОР: Если температура равна или выше максимума — строго чистый КРАСНЫЙ цвет дисплея (RGB565)
    if (value >= maxTemp)
    {
        return 0xF800; // Чистый красный цвет в формате RGB565 (11111 000000 00000)
    }

    // ЖЕСТКИЙ СТОПОР: Если температура ниже или равна минимуму — строго чистый СИНИЙ цвет
    if (value <= minTemp)
    {
        return 0x001F; // Чистый синий цвет в формате RGB565 (00000 000000 11111)
    }

    float factor = 0.0f;
    float hue = 120.0f; // По умолчанию чистый зеленый (120 градусов в HSV)

    if (value < targetTemp)
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

    // Быстрая конвертация Hue в формат RGB565 для дисплея
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
    }

    uint16_t r565 = (((uint16_t)(r * 31)) << 11) | (((uint16_t)(g * 63)) << 5) | ((uint16_t)(b * 31));
    return r565;
}

void DisplayManager::updateMetrics(float coolant, float oil, float coolantR, float transmission)
{
    char buf[12];

    // Моторное масло: читаем пороги из config.oil
    uint16_t oilColor = getTemperatureColor(oil, config.oil.min, config.oil.target, config.oil.max);
    tft.setTextColor(oilColor, TFT_BLACK);
    sprintf(buf, "%-5.0f", oil);
    tft.drawString(buf, 10, 87, 4);

    // Антифриз ДВС: читаем пороги из config.coolant
    uint16_t coolantColor = getTemperatureColor(coolant, config.coolant.min, config.coolant.target, config.coolant.max);
    tft.setTextColor(coolantColor, TFT_BLACK);
    sprintf(buf, "%-5.0f", coolant);
    tft.drawString(buf, 70, 87, 4);

    // Антифриз радиатора: читаем пороги из config.radiator
    uint16_t radiatorColor = getTemperatureColor(coolantR, config.radiator.min, config.radiator.target, config.radiator.max);
    tft.setTextColor(radiatorColor, TFT_BLACK);
    sprintf(buf, "%-5.0f", coolantR);
    tft.drawString(buf, 130, 87, 4);

    // Масло коробки: читаем пороги из config.transmission
    uint16_t transmissionColor = getTemperatureColor(transmission, config.transmission.min, config.transmission.target, config.transmission.max);
    tft.setTextColor(transmissionColor, TFT_BLACK);
    sprintf(buf, "%-5.0f", transmission);
    tft.drawString(buf, 190, 87, 4);
}
