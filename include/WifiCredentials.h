#pragma once

#include <Arduino.h>

constexpr const char *WIFI_DEFAULT_SSID = "QX50Monitoring";
constexpr const char *WIFI_DEFAULT_PASSWORD = "infiniti";
constexpr size_t WIFI_SSID_MAX_BYTES = 32;
constexpr size_t WIFI_PASSWORD_MIN_BYTES = 8;
constexpr size_t WIFI_PASSWORD_MAX_BYTES = 63;

enum class WifiCredentialsError : uint8_t {
    NONE = 0,
    EMPTY_SSID,
    SSID_TOO_LONG,
    PASSWORD_TOO_SHORT,
    PASSWORD_TOO_LONG,
    PASSWORD_INVALID_CHARACTERS,
};

// Проверить ограничения ESP32 для имени и пароля точки доступа
bool wifi_credentials_validate(const String &ssid, const String &password,
                               WifiCredentialsError *error = nullptr);

// Вернуть стабильное имя ошибки для журнала и HTTP-ответа
const char *wifi_credentials_error_name(WifiCredentialsError error);
