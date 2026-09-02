#include "ObdPollPlan.h"

ObdPollPlan::ObdPollPlan(const ObdPidCatalog &catalog, const uint8_t *pids,
                         uint8_t count, uint32_t interval_ms)
    : catalog_(catalog), pids_(pids), count_(count), index_(count),
      interval_ms_(interval_ms), cycle_started_ms_(0), waiting_send_(false)
{
}

int16_t ObdPollPlan::next(uint32_t now, bool primary_due)
{
    if (primary_due) return -1;
    if (waiting_send_) return pids_[index_];

    if (index_ >= count_) {
        if (now - cycle_started_ms_ < interval_ms_) return -1;
        cycle_started_ms_ = now;
        index_ = 0;
    }

    while (index_ < count_ && !catalog_.supports(pids_[index_])) index_++;
    if (index_ >= count_) return -1;

    waiting_send_ = true;
    return pids_[index_];
}

void ObdPollPlan::complete_send(bool sent)
{
    if (!waiting_send_) return;
    if (sent) index_++;
    waiting_send_ = false;
}
