#include "WifiCredentials.h"

bool wifi_credentials_validate(const String &ssid, const String &password, WifiCredentialsError *error)
{
    WifiCredentialsError result = WifiCredentialsError::NONE;

    if (ssid.length() == 0) {
        result = WifiCredentialsError::EMPTY_SSID;
    } else if (ssid.length() > WIFI_SSID_MAX_BYTES) {
        result = WifiCredentialsError::SSID_TOO_LONG;
    } else if (password.length() > 0 && password.length() < WIFI_PASSWORD_MIN_BYTES) {
        result = WifiCredentialsError::PASSWORD_TOO_SHORT;
    } else if (password.length() > WIFI_PASSWORD_MAX_BYTES) {
        result = WifiCredentialsError::PASSWORD_TOO_LONG;
    } else {
        for (size_t i = 0; i < password.length(); ++i) {
            const char c = password[i];
            const bool latin_letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            const bool digit = c >= '0' && c <= '9';
            if (!latin_letter && !digit) {
                result = WifiCredentialsError::PASSWORD_INVALID_CHARACTERS;
                break;
            }
        }
    }

    if (error != nullptr) *error = result;
    return result == WifiCredentialsError::NONE;
}

const char *wifi_credentials_error_name(WifiCredentialsError error)
{
    switch (error) {
        case WifiCredentialsError::NONE:
            return "none";
        case WifiCredentialsError::EMPTY_SSID:
            return "empty_ssid";
        case WifiCredentialsError::SSID_TOO_LONG:
            return "ssid_too_long";
        case WifiCredentialsError::PASSWORD_TOO_SHORT:
            return "password_too_short";
        case WifiCredentialsError::PASSWORD_TOO_LONG:
            return "password_too_long";
        case WifiCredentialsError::PASSWORD_INVALID_CHARACTERS:
            return "password_invalid_characters";
        default:
            return "unknown";
    }
}
