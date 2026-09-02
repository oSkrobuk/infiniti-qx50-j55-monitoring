#pragma once

#include <stdint.h>

#include "ObdPidCatalog.h"

class ObdPollPlan {
public:
    ObdPollPlan(const ObdPidCatalog &catalog, const uint8_t *pids, uint8_t count,
                uint32_t interval_ms);

    int16_t next(uint32_t now, bool primary_due);
    void complete_send(bool sent);

private:
    const ObdPidCatalog &catalog_;
    const uint8_t *pids_;
    uint8_t count_;
    uint8_t index_;
    uint32_t interval_ms_;
    uint32_t cycle_started_ms_;
    bool waiting_send_;
};
