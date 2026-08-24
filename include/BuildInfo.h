#pragma once

// Окружение сборки — по нему веб-интерфейс решает, годится ли автоматическое
// обновление для этой платы. WT32 получает firmware_s3.bin в релизе, но пока
// не публикуется в ветку firmware, поэтому автоустановка для нее не предлагается
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
