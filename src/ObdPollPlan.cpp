#include "ObdPollPlan.h"

ObdPollPlan::ObdPollPlan(const ObdPidCatalog &catalog, const uint8_t *pids,
                         uint8_t count, uint32_t interval_ms, const bool *enabled)
    : catalog_(catalog), pids_(pids), enabled_(enabled), count_(count), index_(count),
      interval_ms_(interval_ms), last_sent_ms_(0), pending_now_ms_(0), waiting_send_(false)
{
}

int16_t ObdPollPlan::next(uint32_t now, bool primary_due)
{
    if (primary_due) return -1;
    if (waiting_send_) return pids_[index_];
    if (now - last_sent_ms_ < interval_ms_) return -1;

    if (index_ >= count_) index_ = 0;
    uint8_t checked = 0;
    while (checked < count_ &&
           (!catalog_.supports(pids_[index_]) || (enabled_ != nullptr && !enabled_[pids_[index_]]))) {
        index_ = (index_ + 1) % count_;
        checked++;
    }
    if (checked == count_) return -1;

    waiting_send_ = true;
    pending_now_ms_ = now;
    return pids_[index_];
}

void ObdPollPlan::complete_send(bool sent)
{
    if (!waiting_send_) return;
    if (sent) {
        index_ = (index_ + 1) % count_;
        last_sent_ms_ = pending_now_ms_;
    }
    waiting_send_ = false;
}

void ObdPollPlan::set_enabled(const bool *enabled)
{
    enabled_ = enabled;
}
