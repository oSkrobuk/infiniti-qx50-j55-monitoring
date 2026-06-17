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
-- **Chatting in Russian.**

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
