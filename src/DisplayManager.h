#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft;
    const int _ledPin;
    const int _ledChannel;
    const int _ledFreq;
    const int _ledResolution;

public:
    // Конструктор с настройками ШИМ по умолчанию
    DisplayManager(int ledPin = 5, int ledChannel = 0, int ledFreq = 20000, int ledResolution = 8);

    // Инициализация экрана и подсветки
    void init();

    // Управление яркостью (0 - 255)
    void setBrightness(uint8_t brightness);

    // Вывод тестового текста
    void showDemoText(const char* title, const char* subtitle);

    // Доступ к объекту TFT для вызова родных функций библиотеки напрямую
    TFT_eSPI& getTft() { return tft; }
};
