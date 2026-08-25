# Agent Rules — Infiniti QX50 J55 Monitoring

## Code Style

All C++ source files (`.cpp`, `.h`) in this project **must** follow the
[Espressif IoT Development Framework Style Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/style-guide.html).

### Main Formatting Rules

| Rule | Convention |
|------|------------|
| Indentation | 4 spaces, no tabs |
| Opening brace in a function definition | On a **new line** (Allman style) |
| Opening brace in a control statement (`if`/`else`/`while`/`for`/`switch`) | On the **same line** (K&R style) |
| Line length | No more than 120 characters |
| `public:` / `private:` labels | At the **same** indentation level as the `class` keyword |
| Opening brace of a class | On the **same line** as the class name |

### Naming Conventions

| Kind | Convention | Example |
|------|------------|---------|
| Classes / structures | `PascalCase` | `CanBusManager`, `CanFrame` |
| Public and private methods | `snake_case` | `get_rpm_color()`, `init()` |
| Free functions | `snake_case` | `can_print_frame()` |
| Local variables | `snake_case` | `green_start`, `file_hash` |
| Private member variables | `snake_case_` (trailing `_`) | `running_`, `rx_count_` |
| File-level `static` variables | `s_` prefix + `snake_case` | `s_fs_mounted`, `s_ap_ssid` |
| Global (`extern`) variables | `snake_case` | `can_bus`, `config` |
| `constexpr` / compile-time constants | `UPPER_SNAKE_CASE` | `CAN_TX_PIN`, `CAN_TIMING` |

### Include Order

Separate the following groups with blank lines:

1. Corresponding header (`"WebManager.h"`)
2. Standard C/C++ headers (`<math.h>`, `<cstdint>`)
3. Third-party / Arduino headers (`<Arduino.h>`, `<WiFi.h>`)
4. Project headers (`"ConfigManager.h"`)

### Other Rules

- Use `#pragma once` in all header files instead of include guards
- Prefer `static_cast<type>()` over C-style casts
- Keep all `.h` files in `include/` and all `.cpp` files in `src/`
- HTML/CSS/JS inside `PROGMEM` strings does not follow the C++ style rules
- Use `//` for single-line comments; use `//` or `/* */` for multi-line comments
- File-level `static` variables must use the `s_` prefix, for example `static bool s_fs_mounted`
- **All comments in `.cpp` and `.h` files must be written in Russian**
- **Do not put a period (`.`) at the end of comments**
- **Do not put a period (`.`) at the end of `Serial.print`/`Serial.println`/`Serial.printf` string literals**
- **Communicate with the user in Russian**

## Building

PlatformIO CLI is not in `PATH`. Its executable is located here on Windows:

```
C:\Users\homework\.platformio\penv\Scripts\pio.exe
```

Convenient PowerShell commands using an environment variable:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run                        # build all firmware variants
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32s3-wt32         # build one environment
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32 -e esp32-mock  # build several environments
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32s3-wt32 -v      # verbose output with compiler flags
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32 -t upload      # flash a device
```

### Environments

| Environment | Board | Display | Data |
|-------------|-------|---------|------|
| `esp32` | ESP32 DEVKIT1 | ST7789 240×240 (SPI) | Real CAN data |
| `esp32-mock` | ESP32 DEVKIT1 | ST7789 240×240 (SPI) | Simulated (`USE_MOCK_DATA`) |
| `esp32s3-wt32` | WT32-SC01 Plus (ESP32-S3) | ST7796 320×480 (parallel 8-bit) | Real CAN data |
| `esp32s3-wt32-mock` | WT32-SC01 Plus (ESP32-S3) | ST7796 320×480 (parallel 8-bit) | Simulated (`USE_MOCK_DATA`) |
| `native` | Host (gcc/clang) | — | Unit tests, see below |

`default_envs` in `platformio.ini` lists only the four firmware environments, so
`pio run` without arguments does not touch `native`.

> `TOUCH_CS` is defined **only** in SPI environments. In parallel 8-bit mode (S3),
> TFT_eSPI does not include touch input declarations, but it compiles `Touch.cpp`
> under `#ifdef TOUCH_CS`. Therefore, `TOUCH_CS` must not be defined there, or the build will fail

## Tests

Unit tests are built on the host with regular gcc and run in seconds. No board or xtensa toolchain is required.

The easiest way to run them is with the script in the project root. It configures the PlatformIO and compiler
paths automatically and passes its own arguments to `pio test` unchanged:

```powershell
.\run-tests.ps1                     # all suites
.\run-tests.ps1 -f test_ota_image   # one suite
```

The equivalent direct commands are:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native                        # all suites
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native -f test_alert_manager  # one suite
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native -vvv                   # verbose output
```

A host compiler must be available in `PATH`. This machine has WinLibs MinGW-w64 installed here:

```
C:\Users\homework\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin
```

### Known Test Exception

- Ignore the `test/test_can_diag` error `Nothing to build`. This is an empty test suite and must not be treated
  as a failed project test

### How It Works

The `native` environment builds only firmware modules that do not depend on hardware (`build_src_filter` in
`platformio.ini`). Missing Arduino headers are replaced with stubs from `test/stubs`:

| Stub | Replacement behavior |
|------|----------------------|
| `Arduino.h` | Test-controlled `millis()` (`mock_set_millis`, `mock_advance_millis`), silent `Serial`, and GPIO no-ops |
| `WString.h` | `String` = `std::string`, which ArduinoJson supports natively |
| `Print.h` / `Stream.h` | Base classes used by ArduinoJson to select Writer and Reader implementations |
| `LittleFS.h` | In-memory filesystem plus `mock_fs_*` flags for simulating flash failures |
| `BuzzerStub.h` | Global `buzzer` and trigger counter |

### Rules for Testable Code

- Logic intended for unit testing must not pull in `<driver/twai.h>`, `<TFT_eSPI.h>`, `<WebServer.h>`, or
  `<esp_ota_ops.h>`. This is why the CAN decoder lives in `CanDecoder.cpp`, color zones in `MetricColors.cpp`,
  and firmware image parsing in `OtaImage.cpp`, while partition operations remain in `OtaSlots.cpp`
- Add every new file that needs tests to `build_src_filter` in the `native` environment
- Include `BuzzerStub.h` exactly once, from the main file of a test suite. Its definitions are intentionally
  not `inline`

## Project Structure

```
include/        C++ header files
src/            C++ implementation files
test/           Unit tests (PlatformIO + Unity)
test/stubs/     Host stubs for Arduino.h, LittleFS.h, and other files
docs/           Documentation and images
partitions.csv  ESP32 OTA partition table
platformio.ini  PlatformIO build configuration
run-tests.ps1   Unit test runner (wrapper around pio test -e native)
AGENTS.md       Agent rules (this file)
```

## Continuous Integration (CI)

`.github/workflows/build.yml` runs strictly in this order:

1. **Unit tests** (`pio test -e native`) plus firmware version verification against the tag. No firmware is
   built until this job passes
2. **Four firmware builds** run in parallel with `fail-fast: false`, so one run exposes all failures. Any failed
   build makes the entire workflow fail
3. **Draft release** runs only for a `v*` tag through `needs: build`
4. **Firmware publication** to the `firmware` branch also runs only for a `v*` tag. The branch has no parent
   history and is recreated with a force push. It contains `<version>/firmware.bin` for ESP32 DEVKIT1 and
   `<version>/firmware_s3.bin` for WT32-SC01 Plus. The web UI's Download and Install button fetches the correct
   image from there because GitHub release assets are served without CORS access for browser JavaScript

## Hardware

- **Microcontroller**: ESP32 (espressif32 @ 6.7.0, Arduino framework)
- **Display**: ST7789 240×240 TFT (TFT_eSPI, SPI)
- **CAN**: SN65HVD230 / WVCMCU-230, TWAI controller, 500 kbit/s, `TWAI_MODE_NORMAL`. The device receives frames
  and sends UDS requests (`0x22`, Tester Present) to the ECM/TCM modules
- **Storage**: LittleFS for persistent configuration
- **OTA**: Dual-partition layout (ota_0 / ota_1, 1.75 MB each)
- **Web UI**: Built-in ESP32 WebServer on port 80, WiFi SoftAP
