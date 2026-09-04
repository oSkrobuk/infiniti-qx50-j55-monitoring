#pragma once

#include <stdint.h>

#include "ObdPidCatalog.h"

class ObdPollPlan {
public:
    ObdPollPlan(const ObdPidCatalog &catalog, const uint8_t *pids, uint8_t count,
                uint32_t cycle_interval_ms, uint32_t request_spacing_ms,
                const bool *enabled = nullptr);

    int16_t next(uint32_t now, bool primary_due);
    void complete_send(bool sent);
    void set_enabled(const bool *enabled);
    void set_request_spacing(uint32_t request_spacing_ms);

private:
    const ObdPidCatalog &catalog_;
    const uint8_t *pids_;
    const bool *enabled_;
    uint8_t count_;
    uint8_t index_;
    uint8_t remaining_;
    uint32_t cycle_interval_ms_;
    uint32_t request_spacing_ms_;
    uint32_t last_cycle_ms_;
    uint32_t last_sent_ms_;
    uint32_t pending_now_ms_;
    bool cycle_active_;
    bool waiting_send_;
};
