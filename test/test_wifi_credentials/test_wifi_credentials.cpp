#include <unity.h>

#include "BuzzerStub.h"
#include "WifiCredentials.h"

void setUp() {}
void tearDown() {}

static void test_factory_credentials_are_valid()
{
    TEST_ASSERT_TRUE(wifi_credentials_validate(WIFI_DEFAULT_SSID, WIFI_DEFAULT_PASSWORD));
}

static void test_open_network_is_valid()
{
    TEST_ASSERT_TRUE(wifi_credentials_validate("QX50", ""));
}

static void test_six_digit_password_is_rejected()
{
    WifiCredentialsError error = WifiCredentialsError::NONE;
    TEST_ASSERT_FALSE(wifi_credentials_validate("QX50", "123456", &error));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiCredentialsError::PASSWORD_TOO_SHORT),
                            static_cast<uint8_t>(error));
}

static void test_password_boundaries_are_enforced()
{
    TEST_ASSERT_TRUE(wifi_credentials_validate("QX50", "12345678"));
    TEST_ASSERT_TRUE(wifi_credentials_validate(
        "QX50", "111111111111111111111111111111111111111111111111111111111111111"));
    TEST_ASSERT_FALSE(wifi_credentials_validate(
        "QX50", "1111111111111111111111111111111111111111111111111111111111111111"));
}

static void test_password_special_characters_are_rejected()
{
    WifiCredentialsError error = WifiCredentialsError::NONE;
    TEST_ASSERT_FALSE(wifi_credentials_validate("QX50", "abcd-1234", &error));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiCredentialsError::PASSWORD_INVALID_CHARACTERS),
                            static_cast<uint8_t>(error));
    TEST_ASSERT_FALSE(wifi_credentials_validate("QX50", "пароль123", &error));
    TEST_ASSERT_TRUE(wifi_credentials_validate("QX50", "Abcd1234"));
}

static void test_every_non_alphanumeric_ascii_character_is_rejected()
{
    for (int value = 1; value <= 127; ++value) {
        const char character = static_cast<char>(value);
        const bool latin_letter = (character >= 'A' && character <= 'Z') ||
                                  (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';
        if (latin_letter || digit) continue;

        String password = "Abcd123";
        password += character;
        WifiCredentialsError error = WifiCredentialsError::NONE;
        TEST_ASSERT_FALSE(wifi_credentials_validate("QX50", password, &error));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiCredentialsError::PASSWORD_INVALID_CHARACTERS),
                                static_cast<uint8_t>(error));
    }
}

static void test_ssid_boundaries_are_enforced_in_bytes()
{
    TEST_ASSERT_TRUE(wifi_credentials_validate("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "12345678"));
    TEST_ASSERT_FALSE(wifi_credentials_validate("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "12345678"));
    TEST_ASSERT_FALSE(wifi_credentials_validate("", "12345678"));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_factory_credentials_are_valid);
    RUN_TEST(test_open_network_is_valid);
    RUN_TEST(test_six_digit_password_is_rejected);
    RUN_TEST(test_password_boundaries_are_enforced);
    RUN_TEST(test_password_special_characters_are_rejected);
    RUN_TEST(test_every_non_alphanumeric_ascii_character_is_rejected);
    RUN_TEST(test_ssid_boundaries_are_enforced_in_bytes);
    return UNITY_END();
}
