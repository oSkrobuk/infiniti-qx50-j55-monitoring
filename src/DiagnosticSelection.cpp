#include "DiagnosticSelection.h"

#include <string.h>

DiagnosticSelection diagnostic_selection;

DiagnosticSelection::DiagnosticSelection()
{
    reset();
}

void DiagnosticSelection::reset()
{
    memset(pids_, 0, sizeof(pids_));
}

void DiagnosticSelection::set_pid(uint8_t pid, bool enabled)
{
    pids_[pid] = enabled;
}

bool DiagnosticSelection::pid_enabled(uint8_t pid) const
{
    return pids_[pid];
}

const bool *DiagnosticSelection::pid_flags() const
{
    return pids_;
}
