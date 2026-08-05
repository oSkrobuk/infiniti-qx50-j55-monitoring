#pragma once

#include <Arduino.h>

// Сведения об одном OTA-разделе для веб-интерфейса
struct OtaSlotInfo {
    const char *label;   // метка раздела: ota_0 или ota_1
    bool     valid;      // в разделе лежит образ приложения
    bool     running;    // из этого раздела работает прошивка прямо сейчас
    bool     boot;       // из этого раздела устройство загрузится после перезагрузки
    bool     known;      // версию прошивки в разделе удалось определить
    String   version;    // версия прошивки из маркера образа
    String   env;        // окружение сборки
    String   build;      // дата и время сборки
    uint32_t size;       // размер раздела, байт
    uint32_t used;       // размер образа, байт (0 — раздел пуст)
};

// Заполнить сведения об OTA-разделах, вернуть их количество (не больше max)
size_t ota_slots_collect(OtaSlotInfo *out, size_t max);

// Сделать раздел label загрузочным. При отказе возвращает false и пишет причину в err
bool ota_slots_set_boot(const char *label, String &err);
