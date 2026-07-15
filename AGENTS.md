# Agent Rules — Infiniti QX50 J55 Monitoring

## Purpose

This repository contains PlatformIO firmware for ESP32/ESP32-S3.
It monitors an Infiniti QX50 J55 over CAN using UDS/ISO 14229, renders metrics on TFT displays,
serves a WiFi AP web UI, stores settings in LittleFS, and supports OTA updates.

## Communication

- Chat with users in Russian.
- Keep final replies concise and list changed file paths.
- Run normal local builds/checks without asking.
- Ask only for destructive, hardware-dependent, security-sensitive, or still-ambiguous actions.

## Repository Layout

```text
include/        C++ headers
src/            C++ implementation files
docs/           Documentation and images
stl/            3D model files
partitions.csv  ESP32 OTA/LittleFS partition table
platformio.ini  PlatformIO environments and build flags
README.md       User documentation
AGENTS.md       Agent instructions
```

## External Agent Rules

- No Cursor rules were found in `.cursor/rules/` or `.cursorrules`.
- No Copilot rules were found in `.github/copilot-instructions.md`.
- If those files are added later, read them and merge any repository-specific rules.

## PlatformIO Environments

- `esp32`: ESP32 DEVKIT1, ST7789 240x240, real CAN data.
- `esp32-mock`: ESP32 DEVKIT1, ST7789 240x240, simulated data via `USE_MOCK_DATA`.
- `esp32s3-wt32`: WT32-SC01 Plus ESP32-S3, ST7796 320x480, real CAN data.
- `esp32s3-wt32-mock`: WT32-SC01 Plus ESP32-S3, ST7796 320x480, simulated data.
- Shared platform: `espressif32@6.7.0`, Arduino framework.
- Main libraries: `bodmer/TFT_eSPI`, `bblanchon/ArduinoJson`.

## Build Commands

Run all commands from the repository root.

```bash
pio run -e esp32
pio run -e esp32-mock
pio run -e esp32s3-wt32
pio run -e esp32s3-wt32-mock
pio run
```

- Use `pio run -e esp32-mock` for quick validation without hardware.
- Use `pio run -e esp32s3-wt32-mock` after WT32-SC01/display conditional changes.
- Use `pio run` for release-oriented or cross-target changes.
- Use `pio run -t clean` only when stale artifacts appear to affect results.

## Lint And Static Analysis

```bash
pio check
pio check -e esp32
pio check -e esp32s3-wt32
pio check --skip-packages
```

- There is no dedicated project linter configuration.
- Prefer `pio check -e <env>` for focused analysis.
- Treat warnings in changed code as actionable.
- Do not mass-format unrelated files.

## Test Commands

There is currently no `test/` or `tests/` directory.
Use PlatformIO's test workflow when tests are added.

```bash
pio test -e esp32
pio test -e esp32-mock
pio test -e esp32s3-wt32
pio test -e esp32s3-wt32-mock
pio test -e esp32-mock -f test_name
```

- Single test command: `pio test -e esp32-mock -f test_name`.
- Put tests under `test/test_name/` with `test_*.cpp` files.
- Use `-f` for `test_filter` when running one suite.
- Prefer mock environments when hardware is unavailable.

## Upload And Monitor Commands

```bash
pio run -e esp32 --target upload
pio run -e esp32-mock --target upload
pio run -e esp32s3-wt32 --target upload
pio run -e esp32s3-wt32-mock --target upload
pio device monitor -b 115200
pio device monitor -e esp32
```

- Do not upload to hardware unless the user asked for flashing or it is clearly expected.
- ESP32 DEVKIT1 uses `upload_protocol = esptool` and `upload_speed = 921600`.
- WT32-SC01 Plus uses ESP32-S3 USB Serial/JTAG via `upload_protocol = esp-builtin`.
- OTA binaries are at `.pio/build/<env>/firmware.bin`.

## Core Code Style

- Follow the Espressif IoT Development Framework style guide for `.cpp` and `.h` files.
- Use existing code as the tie-breaker when rules are unclear.
- Indent with 4 spaces and never tabs.
- Keep lines at or below 120 characters where practical.
- Function definition braces use Allman style: opening `{` on a new line.
- Control-flow braces use K&R style: opening `{` on the same line.
- Class opening brace stays on the same line as the class name.
- `public:` and `private:` align with the `class` keyword.

## Header And Source Organization

- Headers live in `include/`; implementation files live in `src/`.
- Use `#pragma once` in headers.
- Keep one main class per header/source pair when practical.
- Match filenames to class names, for example `CanBusManager.h` and `CanBusManager.cpp`.
- Declare global objects as `extern` in headers and define them once in a `.cpp` file.
- Use `static` for file-scope helper functions and variables unless external linkage is required.

## Include Order

Separate include groups with one blank line.

1. Corresponding header, for example `#include "WebManager.h"`.
2. C/C++ standard headers, for example `<math.h>` or `<cstdint>`.
3. Third-party and Arduino headers, for example `<Arduino.h>`, `<WiFi.h>`, `<ArduinoJson.h>`.
4. Project headers, for example `#include "ConfigManager.h"`.

## Naming Conventions

| Kind | Convention | Example |
|------|------------|---------|
| Classes / structs | `PascalCase` | `CanBusManager`, `PollEntry` |
| Methods and free functions | `snake_case` | `save_to_file()`, `can_print_frame()` |
| Local variables | `snake_case` | `poll_interval_ms` |
| Private members | `snake_case_` | `running_` |
| File-scope static variables | `s_` + `snake_case` | `s_last_poll_ms` |
| Global extern objects | `snake_case` | `can_bus`, `config` |
| Constants and build flags | `UPPER_SNAKE_CASE` | `CAN_TX_PIN`, `USE_MOCK_DATA` |

## Types And Casting

- Prefer fixed-width integers for protocol, CAN, timing, and serialized values.
- Use `float` for displayed sensor values, matching the existing firmware.
- Use `const`, `constexpr`, and `static constexpr` for immutable values.
- Use `static_cast<type>()`; do not introduce C-style casts.
- Keep byte order explicit when decoding UDS payloads.
- Validate CAN `dlc` before reading frame bytes.
- Avoid heap-heavy STL containers in firmware paths unless there is a measured reason.

## Comments And Strings

- All comments in `.cpp` and `.h` files must be written in Russian.
- Do not put a period (`.`) at the end of comments.
- Do not put a period (`.`) at the end of `Serial.print`, `Serial.println`, or `Serial.printf` string literals.
- Add comments only for non-obvious protocol, calibration, timing, or hardware behavior.
- Embedded HTML/CSS/JS in `PROGMEM` strings is exempt from C++ comment and formatting rules.

## Error Handling

- Return `bool` for init/load/save/send operations that can fail.
- Check `esp_err_t` results and log failures with `esp_err_to_name(err)`.
- Clean up partially initialized hardware resources before returning failure.
- Avoid silent failures in CAN, LittleFS, OTA, WiFi, and config persistence paths.
- Do not add exceptions; this is Arduino/ESP32 firmware.
- Keep `Serial` logs short enough for embedded monitoring.

## Timing And Embedded Constraints

- Keep `loop()` responsive; avoid long blocking delays and heavy synchronous work.
- Keep CAN receive handling non-blocking to avoid losing frames.
- Preserve UDS polling cadence and `Tester Present` behavior unless intentionally changing protocol timing.
- Use `millis()`-based scheduling for repeated tasks.
- Rate-limit display refreshes, alert checks, and LittleFS writes.
- Consider RAM, flash size, and OTA partition limits before adding embedded assets.

## Hardware, Config, And Docs

- CAN transceiver: SN65HVD230 / WVCMCU-230 at 500 kbit/s.
- CAN pins come from build flags: `CAN_TX_PIN_NUM=32`, `CAN_RX_PIN_NUM=34`; buzzer is `BUZZER_PIN_NUM=26`.
- ECM request/response IDs: `0x7E0` / `0x7E8`; TCM request/response IDs: `0x7E1` / `0x7E9`.
- UDS requests use service `0x22`; positive responses use `0x62`.
- `ConfigManager` owns `/config.json`; alerts use `/checks.json` and `/alerts.json`.
- Web UI is embedded in `WebManager.cpp`; preserve Russian user-facing text and mobile usability.
- Update `README.md` when changing wiring, environments, flashing, APIs, metrics, alerts, or OTA behavior.
- Never commit `.pio/`, local device state, credentials, or private WiFi values.

## Git Hygiene And Validation

- The worktree may contain user changes; never revert them unless explicitly requested.
- Do not run destructive git commands such as `git reset --hard` or `git checkout --` without approval.
- Do not commit or push unless the user explicitly asks.
- For shared C++ changes, run `pio run -e esp32-mock`.
- For display/platform conditionals, also run the affected mock target.
- If validation cannot be run, state why and provide the exact command.
