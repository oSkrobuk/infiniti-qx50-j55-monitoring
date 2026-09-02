#include "CanRecovery.h"

#include <algorithm>

CanRecovery::CanRecovery()
{
    reset();
}

void CanRecovery::reset()
{
    state_ = CanDriverState::STOPPED;
    bus_off_count_ = 0;
    recovery_count_ = 0;
    initiate_attempt_count_ = 0;
    initiate_failure_count_ = 0;
    start_attempt_count_ = 0;
    start_failure_count_ = 0;
    next_attempt_ts_ = 0;
    backoff_ms_ = INITIAL_BACKOFF_MS;
}

CanRecoveryAction CanRecovery::observe(CanDriverState driver_state, uint32_t now_ms)
{
    if (driver_state == CanDriverState::RUNNING) {
        state_ = CanDriverState::RUNNING;
        next_attempt_ts_ = 0;
        backoff_ms_ = INITIAL_BACKOFF_MS;
        return CanRecoveryAction::NONE;
    }

    if (driver_state == CanDriverState::BUS_OFF) {
        // После принятого initiate драйвер некоторое время еще может сообщать BUS_OFF
        if (state_ == CanDriverState::RECOVERING) return CanRecoveryAction::NONE;

        if (state_ != CanDriverState::BUS_OFF) {
            state_ = CanDriverState::BUS_OFF;
            bus_off_count_++;
            next_attempt_ts_ = now_ms;
            backoff_ms_ = INITIAL_BACKOFF_MS;
        }
        return ready(now_ms) ? CanRecoveryAction::INITIATE : CanRecoveryAction::NONE;
    }

    if (driver_state == CanDriverState::RECOVERING) {
        state_ = CanDriverState::RECOVERING;
        return CanRecoveryAction::NONE;
    }

    if (state_ == CanDriverState::RECOVERING) {
        return ready(now_ms) ? CanRecoveryAction::START : CanRecoveryAction::NONE;
    }

    state_ = CanDriverState::STOPPED;
    return CanRecoveryAction::NONE;
}

void CanRecovery::complete(CanRecoveryAction action, bool success, uint32_t now_ms)
{
    if (action == CanRecoveryAction::INITIATE) {
        initiate_attempt_count_++;
        if (success) {
            state_ = CanDriverState::RECOVERING;
            next_attempt_ts_ = 0;
            backoff_ms_ = INITIAL_BACKOFF_MS;
        } else {
            initiate_failure_count_++;
            state_ = CanDriverState::BUS_OFF;
            schedule_retry(now_ms);
        }
        return;
    }

    if (action == CanRecoveryAction::START) {
        start_attempt_count_++;
        if (success) {
            recovery_count_++;
            state_ = CanDriverState::RUNNING;
            next_attempt_ts_ = 0;
            backoff_ms_ = INITIAL_BACKOFF_MS;
        } else {
            start_failure_count_++;
            state_ = CanDriverState::RECOVERING;
            schedule_retry(now_ms);
        }
    }
}

CanDriverState CanRecovery::state() const
{
    return state_;
}

uint32_t CanRecovery::bus_off_count() const
{
    return bus_off_count_;
}

uint32_t CanRecovery::recovery_count() const
{
    return recovery_count_;
}

uint32_t CanRecovery::initiate_attempt_count() const
{
    return initiate_attempt_count_;
}

uint32_t CanRecovery::initiate_failure_count() const
{
    return initiate_failure_count_;
}

uint32_t CanRecovery::start_attempt_count() const
{
    return start_attempt_count_;
}

uint32_t CanRecovery::start_failure_count() const
{
    return start_failure_count_;
}

uint32_t CanRecovery::next_attempt_ts() const
{
    return next_attempt_ts_;
}

bool CanRecovery::ready(uint32_t now_ms) const
{
    return static_cast<int32_t>(now_ms - next_attempt_ts_) >= 0;
}

void CanRecovery::schedule_retry(uint32_t now_ms)
{
    next_attempt_ts_ = now_ms + backoff_ms_;
    backoff_ms_ = std::min(backoff_ms_ * 2, MAX_BACKOFF_MS);
}
