#pragma once

#include <stdint.h>

static constexpr uint8_t DIAGNOSTIC_MAX_PID = 0xA6;

class DiagnosticSelection {
public:
    DiagnosticSelection();

    void reset();
    void set_pid(uint8_t pid, bool enabled);
    bool pid_enabled(uint8_t pid) const;
    const bool *pid_flags() const;

private:
    bool pids_[256];
};

extern DiagnosticSelection diagnostic_selection;
