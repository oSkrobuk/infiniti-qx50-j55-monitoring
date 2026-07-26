#pragma once

#include "Print.h"

// Минимальная замена Arduino Stream для сборки под хост.
//
// ArduinoJson выбирает Reader по факту наследования от Stream, поэтому важны
// именно read() и readBytes() — через них работает deserializeJson(doc, file)
class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read()      = 0;
    virtual int peek()      = 0;

    virtual size_t readBytes(char *buffer, size_t length)
    {
        size_t count = 0;
        while (count < length) {
            int c = read();
            if (c < 0) break;
            *buffer++ = static_cast<char>(c);
            count++;
        }
        return count;
    }
};
