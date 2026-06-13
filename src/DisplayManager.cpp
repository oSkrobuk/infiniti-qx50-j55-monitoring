#include "DisplayManager.h"

DisplayManager::DisplayManager(int ledPin, int ledChannel, int ledFreq, int ledResolution)
    : _ledPin(ledPin), _ledChannel(ledChannel), _ledFreq(ledFreq), _ledResolution(ledResolution) {}

void DisplayManager::init() {
    // Настройка аппаратного ШИМ подсветки на ESP32
    ledcAttachChannel(_ledPin, _ledFreq, _ledResolution, _ledChannel);
    ledcWrite(_ledPin, 0); // Держим экран темным при старте

    // Инициализация матрицы дисплея
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    ledcWrite(_ledPin, brightness);
}

void DisplayManager::showDemoText(const char* title, const char* subtitle) {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(title, 20, 100);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(subtitle, 50, 130);
}
