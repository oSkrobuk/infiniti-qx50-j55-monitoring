#pragma once

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "Print.h"
#include "Stream.h"
#include "WString.h"

// Замена Arduino.h для сборки под хост (окружение native).
//
// Подключается вместо настоящего заголовка за счет -I test/stubs: в окружении
// native фреймворка Arduino нет вообще, поэтому конфликта имен не возникает

// ── Управляемое время ────────────────────────────────────────────────────────
//
// Прошивка меряет интервалы через millis(). Под хостом время не идет само:
// тест двигает его руками. Благодаря этому антидребезг алертов
// (ALERT_RETRIGGER_MS) и время показа оверлея (ALERT_DISPLAY_MS) проверяются
// детерминированно, без sleep и без гонок

inline unsigned long mock_millis_value = 0;

inline unsigned long millis()
{
    return mock_millis_value;
}

inline unsigned long micros()
{
    return mock_millis_value * 1000UL;
}

// Установить абсолютное значение millis()
inline void mock_set_millis(unsigned long ms)
{
    mock_millis_value = ms;
}

// Сдвинуть millis() вперед на delta мс
inline void mock_advance_millis(unsigned long delta)
{
    mock_millis_value += delta;
}

inline void delay(unsigned long ms)
{
    mock_millis_value += ms;
}

inline void delayMicroseconds(unsigned int) {}

// ── Заглушки GPIO ────────────────────────────────────────────────────────────

#define LOW          0
#define HIGH         1
#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int  digitalRead(uint8_t) { return 0; }
inline int  analogRead(uint8_t) { return 0; }

// ── Заглушка Serial ──────────────────────────────────────────────────────────
//
// Молчит намеренно: диагностический вывод прошивки затопил бы отчет Unity

class MockSerial : public Print {
public:
    void begin(unsigned long = 115200) {}
    void end() {}
    void flush() {}
    int  available() { return 0; }
    int  read() { return -1; }

    size_t write(uint8_t) override { return 1; }
    size_t write(const uint8_t *, size_t size) override { return size; }

    template <typename T>
    size_t print(const T &) { return 0; }

    template <typename T>
    size_t println(const T &) { return 0; }

    size_t println() { return 0; }
    size_t printf(const char *, ...) { return 0; }
};

inline MockSerial Serial;

// ── PROGMEM ──────────────────────────────────────────────────────────────────

#define PROGMEM
#define PGM_P const char *
#define F(x)  (x)
