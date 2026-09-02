#include "DiagnosticMode.h"

static uint32_t s_last_touch_ms = 0;
static bool s_touched = false;

void diagnostic_mode_touch(uint32_t now)
{
    s_last_touch_ms = now;
    s_touched = true;
}

bool diagnostic_mode_active(uint32_t now)
{
    return s_touched && now - s_last_touch_ms <= DIAGNOSTIC_MODE_TIMEOUT_MS;
}

void diagnostic_mode_reset()
{
    s_last_touch_ms = 0;
    s_touched = false;
}
