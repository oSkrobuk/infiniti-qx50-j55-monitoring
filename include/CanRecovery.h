#pragma once

#include <cstdint>

enum class CanDriverState : uint8_t {
    STOPPED,
    RUNNING,
    BUS_OFF,
    RECOVERING,
};

enum class CanRecoveryAction : uint8_t {
    NONE,
    INITIATE,
    START,
};

class CanRecovery {
public:
    static constexpr uint32_t INITIAL_BACKOFF_MS = 250;
    static constexpr uint32_t MAX_BACKOFF_MS = 8000;

    CanRecovery();

    void reset();
    CanRecoveryAction observe(CanDriverState driver_state, uint32_t now_ms);
    void complete(CanRecoveryAction action, bool success, uint32_t now_ms);

    CanDriverState state() const;
    uint32_t bus_off_count() const;
    uint32_t recovery_count() const;
    uint32_t initiate_attempt_count() const;
    uint32_t initiate_failure_count() const;
    uint32_t start_attempt_count() const;
    uint32_t start_failure_count() const;
    uint32_t next_attempt_ts() const;

private:
    CanDriverState state_;
    uint32_t bus_off_count_;
    uint32_t recovery_count_;
    uint32_t initiate_attempt_count_;
    uint32_t initiate_failure_count_;
    uint32_t start_attempt_count_;
    uint32_t start_failure_count_;
    uint32_t next_attempt_ts_;
    uint32_t backoff_ms_;

    bool ready(uint32_t now_ms) const;
    void schedule_retry(uint32_t now_ms);
};
