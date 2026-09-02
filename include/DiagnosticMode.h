#pragma once

#include <stdint.h>

static constexpr uint32_t DIAGNOSTIC_MODE_TIMEOUT_MS = 3000;

void diagnostic_mode_touch(uint32_t now);
bool diagnostic_mode_active(uint32_t now);
void diagnostic_mode_reset();
