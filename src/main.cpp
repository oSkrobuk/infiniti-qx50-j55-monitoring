#include <Arduino.h>
#include "DisplayManager.h"

// Создаем объект менеджера дисплея (пин D5 применится автоматически)
DisplayManager display;

void setup() {
    Serial.begin(115200);

    // Инициализируем дисплей через наш класс
    display.init();

    // Выводим текст
    display.showDemoText("ESP32 + ST7789", "PWM Ready!");

    // Плавно зажигаем экран
    for (int brightness = 0; brightness <= 255; brightness++) {
        display.setBrightness(brightness);
        delay(4);
    }
}

void loop() {
    // Демонстрация работы ШИМ через методы нашего класса
    
    // Плавное затухание
    for (int brightness = 255; brightness >= 25; brightness--) {
        display.setBrightness(brightness);
        delay(10);
    }
    delay(1000);

    // Плавное разгорание
    for (int brightness = 25; brightness <= 255; brightness++) {
        display.setBrightness(brightness);
        delay(10);
    }
    delay(3000);
}
