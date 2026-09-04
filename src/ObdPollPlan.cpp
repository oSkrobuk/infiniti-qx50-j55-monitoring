#include "ObdPollPlan.h"

ObdPollPlan::ObdPollPlan(const ObdPidCatalog &catalog, const uint8_t *pids,
                         uint8_t count, uint32_t cycle_interval_ms,
                         uint32_t request_spacing_ms, const bool *enabled)
    : catalog_(catalog), pids_(pids), enabled_(enabled), count_(count), index_(0), remaining_(0),
      cycle_interval_ms_(cycle_interval_ms), request_spacing_ms_(request_spacing_ms),
      last_cycle_ms_(0), last_sent_ms_(0), pending_now_ms_(0), cycle_active_(false), waiting_send_(false)
{
}

int16_t ObdPollPlan::next(uint32_t now, bool primary_due)
{
    if (primary_due) return -1;
    if (waiting_send_) return pids_[index_];

    if (!cycle_active_) {
        if (now - last_cycle_ms_ < cycle_interval_ms_) return -1;
        last_cycle_ms_ = now;
        index_ = 0;
        remaining_ = count_;
        cycle_active_ = true;
    }

    if (last_sent_ms_ != 0 && now - last_sent_ms_ < request_spacing_ms_) return -1;

    while (remaining_ > 0 &&
           (!catalog_.supports(pids_[index_]) || (enabled_ != nullptr && !enabled_[pids_[index_]]))) {
        index_ = (index_ + 1) % count_;
        remaining_--;
    }
    if (remaining_ == 0) {
        cycle_active_ = false;
        return -1;
    }

    waiting_send_ = true;
    pending_now_ms_ = now;
    return pids_[index_];
}

void ObdPollPlan::complete_send(bool sent)
{
    if (!waiting_send_) return;
    if (sent) {
        index_ = (index_ + 1) % count_;
        remaining_--;
        last_sent_ms_ = pending_now_ms_;
        if (remaining_ == 0) cycle_active_ = false;
    }
    waiting_send_ = false;
}

void ObdPollPlan::set_enabled(const bool *enabled)
{
    enabled_ = enabled;
}

void ObdPollPlan::set_request_spacing(uint32_t request_spacing_ms)
{
    if (request_spacing_ms < 1) request_spacing_ms = 1;
    if (request_spacing_ms > 100) request_spacing_ms = 100;
    request_spacing_ms_ = request_spacing_ms;
}
