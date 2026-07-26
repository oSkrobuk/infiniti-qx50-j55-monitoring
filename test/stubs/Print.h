#pragma once

#include <cstddef>
#include <cstdint>

// Минимальная замена Arduino Print для сборки под хост.
//
// ArduinoJson выбирает Writer по факту наследования от Print, поэтому важна
// именно эта пара методов write() — через нее работает serializeJson(doc, file)
class Print {
public:
    virtual ~Print() {}

    virtual size_t write(uint8_t c) = 0;

    virtual size_t write(const uint8_t *buffer, size_t size)
    {
        size_t written = 0;
        while (size--) {
            if (write(*buffer++) == 0) break;
            written++;
        }
        return written;
    }
};

// Arduino объявляет Printable рядом с Print. Сама прошивка этот тип не
// использует, но ArduinoJson при включенном ARDUINOJSON_ENABLE_ARDUINO_PRINT
// объявляет для него перегрузку convertToJson() — без объявления сборка падает
class Printable {
public:
    virtual ~Printable() {}

    virtual size_t printTo(Print &p) const = 0;
};
