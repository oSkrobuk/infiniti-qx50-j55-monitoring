#include "DisplayManager.h"

DisplayManager::DisplayManager() : tft(TFT_eSPI()) {}

void DisplayManager::init() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    
    //tft.drawRect(0, 0, 240, 240, TFT_DARKGREY);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("INFINITI QX50 J55", 30, 6, 4);
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.drawString("MONITORING", 75, 31, 4);
    
    //tft.drawFastHLine(0, 56, 240, TFT_DARKGREY);
    
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("Engine Oil", 5, 55, 2);
    tft.drawString("E Coolant", 88, 55, 2);
    tft.drawString("R Coolant", 166, 55, 2);

    tft.drawString("Battery", 5, 110, 2);
}

void DisplayManager::updateMetrics(float coolant, float oil, float voltage, float boost) {
    char buf[12];
    
    // Форматирование строк с пробелами справа (затирает старые цифры)
    // Используем крупный шрифт №4 для вывода значений параметров
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    
    // 1. Температура антифриза
    sprintf(buf, "%-5.0f", coolant);
    tft.drawString(buf, 15, 90, 4);
    
    // 2. Температура масла
    sprintf(buf, "%-5.0f", oil);
    tft.drawString(buf, 130, 90, 4);
    
    // 3. Вольтаж бортовой сети
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    sprintf(buf, "%-5.1f", voltage);
    tft.drawString(buf, 15, 170, 4);
    
    // 4. Давление наддува (турбина)
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    sprintf(buf, "%-5.2f", boost);
    tft.drawString(buf, 130, 170, 4);
}
