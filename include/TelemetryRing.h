#pragma once

#include <stdint.h>

#include "protocol_generated.h"

#pragma pack(push, 1)
struct StoredTelemetryRecord {
    uint32_t uptime_ms;
    int16_t values[METRIC_COUNT];
};
#pragma pack(pop)

static_assert(sizeof(StoredTelemetryRecord) == 20, "telemetry record must remain 20 bytes");

class TelemetryRing {
public:
    TelemetryRing();
    ~TelemetryRing();

    TelemetryRing(const TelemetryRing &) = delete;
    TelemetryRing &operator=(const TelemetryRing &) = delete;

    bool init(uint16_t capacity);
    void reset();
    bool append(const StoredTelemetryRecord &record);
    bool get(uint32_t seq, StoredTelemetryRecord &record) const;
    void ack(uint32_t seq);

    uint16_t capacity() const { return capacity_; }
    uint32_t oldest_seq() const { return oldest_seq_; }
    uint32_t next_seq() const { return next_seq_; }
    uint32_t newest_seq() const { return empty() ? oldest_seq_ : next_seq_ - 1; }
    uint32_t lost_unacked_count() const { return lost_unacked_; }
    bool empty() const { return oldest_seq_ == next_seq_; }

private:
    StoredTelemetryRecord *records_;
    uint16_t capacity_;
    uint32_t oldest_seq_;
    uint32_t next_seq_;
    uint32_t acked_upto_;
    uint32_t lost_unacked_;
};
