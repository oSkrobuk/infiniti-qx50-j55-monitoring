#pragma once

#include <Arduino.h>

// Разбор образа прошивки: маркер с версией и длина образа.
//
// Модуль намеренно не знает ни про esp_partition, ни про WebServer — только
// чистые вычисления над байтами, поэтому собирается под хост и покрыт тестами
// (test/test_ota_image). Работа с разделами живет в OtaSlots.cpp

// Маркер с версией прошивки, вшитый в образ.
//
// Свою версию прошивка знает из FW_VERSION, а вот соседний слот приходится
// опознавать по лежащему там образу. Штатный esp_app_desc для этого не годится:
// в Arduino он приходит из заранее собранного ядра и содержит версию ESP-IDF,
// а не нашу. Поэтому в образ кладется собственная строка вида
// "QX50-FW-TAG:версия|окружение|дата сборки|", которую поиск находит линейным
// чтением чужого раздела
extern const char FW_IMAGE_TAG[];

// Длина сигнатуры "QX50-FW-TAG:" в начале маркера. Отдельным литералом
// сигнатуру не объявляем: тогда в образе оказалось бы два ее вхождения,
// и поиск мог бы наткнуться не на маркер, а на образец для сравнения
constexpr size_t OTA_TAG_SIG_LEN = 12;

// Длиннее маркер не бывает: версия, окружение и дата сборки вместе
// укладываются в полсотни символов
constexpr size_t OTA_TAG_MAX_LEN = 128;

// Длина заголовка образа приложения (esp_image_header_t)
constexpr uint32_t OTA_IMAGE_HEADER_LEN = 24;

// Что записано в маркере
struct OtaTag {
    String version;
    String env;
    String build;
};

// Потоковый поиск маркера во время OTA-загрузки
//
// HTTPUpload отдает образ частями, причем граница части может пройти в любом
// месте маркера. Класс держит только небольшое окно и собранный маркер, поэтому
// размер прошивки не влияет на расход оперативной памяти
class OtaTagStream {
public:
    OtaTagStream();

    void reset();
    void feed(const uint8_t *data, size_t len);

    bool found() const;
    const OtaTag &tag() const;

private:
    uint8_t signature_[OTA_TAG_SIG_LEN];
    size_t  signature_len_;
    char    candidate_[OTA_TAG_MAX_LEN + 1];
    size_t  candidate_len_;
    uint8_t separator_count_;
    bool    collecting_;
    bool    found_;
    OtaTag  tag_;
};

// Разобрать маркер, на который указывает tag; len — сколько байт доступно
// за указателем. В чужом образе за сигнатурой может оказаться что угодно,
// поэтому поля принимаются только из печатного ASCII
bool ota_tag_parse(const char *tag, size_t len, OtaTag &out);

// Проверить совместимость окружений для OTA
// real и mock одной платы совместимы, ESP32 DEVKIT1 и WT32-SC01 Plus — нет
bool ota_envs_compatible(const char *running_env, const char *image_env);

// Найти начало маркера в первых len байтах блока, вернуть смещение или -1.
// Сигнатура должна помещаться в блок целиком
int ota_tag_find(const uint8_t *buf, size_t len);

// Каким блоком читается образ при поиске маркера
constexpr size_t OTA_SCAN_CHUNK = 2048;

// Чтение куска раздела по смещению от его начала. false — прочитать не удалось
using OtaImageReader = bool (*)(const void *ctx, uint32_t offset, void *dst, size_t len);

// Длина образа приложения: заголовок, сегменты, контрольный байт и SHA-256.
// Ноль — образа нет, он поврежден или не помещается в part_size
uint32_t ota_image_length(OtaImageReader read, const void *ctx, uint32_t part_size);

// Найти и разобрать маркер в образе, читая его блоками по OTA_SCAN_CHUNK.
// limit — длина образа: хвост раздела за ней не стерт и может хранить куски
// прошлой прошивки, поэтому дальше поиск не идет
bool ota_tag_scan(OtaImageReader read, const void *ctx, uint32_t limit, OtaTag &out);
