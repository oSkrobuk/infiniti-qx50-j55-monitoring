# Agent Rules — Infiniti QX50 J55 Monitoring

## Code Style

All C++ source files (`.cpp`, `.h`) in this project **must** follow the
[Espressif IoT Development Framework Style Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/style-guide.html).

### Key formatting rules

| Rule | Convention |
|------|------------|
| Indentation | 4 spaces (no tabs) |
| Function definition brace | Opening `{` on a **new line** (Allman) |
| Control-flow brace (`if`/`else`/`while`/`for`/`switch`) | Opening `{` on the **same line** (K&R) |
| Line length | ≤ 120 characters |
| `public:` / `private:` labels | At **same** indentation level as `class` keyword |
| Class opening brace | On the **same line** as the class name |

### Naming conventions

| Kind | Convention | Example |
|------|-----------|---------|
| Classes / structs | `PascalCase` | `CanBusManager`, `CanFrame` |
| Methods (public & private) | `snake_case` | `get_rpm_color()`, `init()` |
| Free-standing functions | `snake_case` | `can_print_frame()` |
| Local variables | `snake_case` | `green_start`, `file_hash` |
| Private member variables | `snake_case_` (trailing `_`) | `running_`, `rx_count_` |
| File-scope `static` variables | `s_` prefix + `snake_case` | `s_fs_mounted`, `s_ap_ssid` |
| Global (extern) variables | `snake_case` | `can_bus`, `config` |
| `constexpr` / compile-time constants | `UPPER_SNAKE_CASE` | `CAN_TX_PIN`, `CAN_TIMING` |

### Includes order (each group separated by a blank line)

1. Corresponding header (`"WebManager.h"`)
2. C/C++ standard headers (`<math.h>`, `<cstdint>`)
3. Third-party / Arduino headers (`<Arduino.h>`, `<WiFi.h>`)
4. Project headers (`"ConfigManager.h"`)

### Other rules

- Use `#pragma once` for all header guards.
- Prefer `static_cast<type>()` over C-style casts.
- All `.h` files live in `include/`, all `.cpp` files in `src/`.
- HTML/CSS/JS embedded in `PROGMEM` strings is exempt from C++ style rules.
- Use `//` for single-line comments; `//` or `/* */` for multi-line.
- File-scope `static` variables must be prefixed with `s_` (e.g., `static bool s_fs_mounted`).
- **All comments in `.cpp` and `.h` files must be written in Russian.**
- **Do NOT put a period (`.`) at the end of comments.**
- **Do NOT put a period (`.`) at the end of `Serial.print`/`Serial.println`/`Serial.printf` string literals.**
-- **Chatting in Russian.**

## Build

PlatformIO CLI не в `PATH`. Исполняемый файл лежит здесь (Windows):

```
C:\Users\homework\.platformio\penv\Scripts\pio.exe
```

Из PowerShell удобно вызывать через переменную окружения:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run                        # собрать все окружения
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32s3-wt32         # одно окружение
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32 -e esp32-mock  # несколько окружений
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32s3-wt32 -v      # подробный вывод (видны флаги компилятора)
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32 -t upload      # прошить
```

### Окружения

| Окружение | Плата | Дисплей | Данные |
|-----------|-------|---------|--------|
| `esp32` | ESP32 DEVKIT1 | ST7789 240×240 (SPI) | реальные из CAN |
| `esp32-mock` | ESP32 DEVKIT1 | ST7789 240×240 (SPI) | имитация (`USE_MOCK_DATA`) |
| `esp32s3-wt32` | WT32-SC01 Plus (ESP32-S3) | ST7796 320×480 (параллельный 8-бит) | реальные из CAN |
| `esp32s3-wt32-mock` | WT32-SC01 Plus (ESP32-S3) | ST7796 320×480 (параллельный 8-бит) | имитация (`USE_MOCK_DATA`) |

> `TOUCH_CS` определяется **только** в SPI-окружениях. В параллельном 8-битном
> режиме (S3) TFT_eSPI не подключает объявления touch, но компилирует `Touch.cpp`
> по `#ifdef TOUCH_CS` — поэтому определять там `TOUCH_CS` нельзя, иначе сборка падает

## Project structure

```
include/        C++ headers
src/            C++ implementation files
docs/           Documentation and images
partitions.csv  ESP32 OTA partition table
platformio.ini  PlatformIO build configuration
AGENTS.md       Agent rules (this file)
```

## Hardware

- **MCU**: ESP32 (espressif32 @ 6.7.0, Arduino framework)
- **Display**: ST7789 240×240 TFT (TFT_eSPI, SPI)
- **CAN**: SN65HVD230 / WVCMCU-230, TWAI controller, 500 kbps, listen-only
- **Storage**: LittleFS for config persistence
- **OTA**: dual-slot partition layout (ota_0 / ota_1, 1.75 MB each)
- **Web**: ESP32 built-in WebServer on port 80, WiFi SoftAP
