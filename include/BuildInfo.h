#pragma once

// Окружение сборки — по нему веб-интерфейс решает, годится ли firmware.bin
// из релиза для этой платы. CI кладет в релиз бинарники только ESP32 DEVKIT1,
// поэтому на WT32-SC01 Plus автоустановка не предлагается.
//
// Макрос, а не constexpr: строка склеивается препроцессором с версией
// в маркер образа (FW_IMAGE_TAG в src/OtaSlots.cpp)
#if defined(DISPLAY_WT32_S3) && defined(USE_MOCK_DATA)
#define BUILD_ENV "esp32s3-wt32-mock"
#elif defined(DISPLAY_WT32_S3)
#define BUILD_ENV "esp32s3-wt32"
#elif defined(USE_MOCK_DATA)
#define BUILD_ENV "esp32-mock"
#else
#define BUILD_ENV "esp32"
#endif

// Дата и время сборки одной строкой
#define BUILD_STAMP __DATE__ " " __TIME__
