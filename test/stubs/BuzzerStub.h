#pragma once

#include "BuzzerController.h"

// Заглушка бузера: настоящий BuzzerController.cpp дергает ledc-периферию ESP32
// и под хостом не собирается, но AlertManager.cpp обращается к глобальному
// объекту buzzer через extern — значит символ обязан существовать при линковке.
//
// Определения намеренно НЕ inline: inline-функцию компилятор выпускает только
// если она вызвана в этой же единице трансляции, а вызов сидит в AlertManager.o.
// Поэтому заголовок подключается ровно один раз — из главного файла каждого
// набора тестов (у каждого набора свой бинарник, конфликта определений нет)

// Сколько раз сработал бузер. По этому счетчику тест видит, что проверка
// действительно отработала, а не просто дописала строку в журнал
uint32_t buzzer_alert_count = 0;

void BuzzerController::trigger_alert()
{
    buzzer_alert_count++;
}

void BuzzerController::update() {}

void BuzzerController::hello() {}

// Глобальный объект, который AlertManager.cpp объявляет через extern
BuzzerController buzzer;
