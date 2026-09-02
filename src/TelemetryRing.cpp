#include "TelemetryRing.h"

#include <new>

TelemetryRing::TelemetryRing()
    : records_(nullptr), capacity_(0), oldest_seq_(0), next_seq_(0), acked_upto_(0),
      lost_unacked_(0)
{
}

TelemetryRing::~TelemetryRing()
{
    delete[] records_;
}

bool TelemetryRing::init(uint16_t capacity)
{
    if (capacity == 0) return false;
    StoredTelemetryRecord *records = new (std::nothrow) StoredTelemetryRecord[capacity];
    if (records == nullptr) return false;
    delete[] records_;
    records_ = records;
    capacity_ = capacity;
    reset();
    return true;
}

void TelemetryRing::reset()
{
    oldest_seq_ = 0;
    next_seq_ = 0;
    acked_upto_ = 0;
    lost_unacked_ = 0;
}

bool TelemetryRing::append(const StoredTelemetryRecord &record)
{
    if (records_ == nullptr) return false;
    bool lost = false;
    if (next_seq_ - oldest_seq_ == capacity_) {
        ++oldest_seq_;
        if (acked_upto_ < oldest_seq_) {
            acked_upto_ = oldest_seq_;
            ++lost_unacked_;
            lost = true;
        }
    }
    records_[next_seq_ % capacity_] = record;
    ++next_seq_;
    return lost;
}

bool TelemetryRing::get(uint32_t seq, StoredTelemetryRecord &record) const
{
    if (records_ == nullptr || seq < oldest_seq_ || seq >= next_seq_) return false;
    record = records_[seq % capacity_];
    return true;
}

void TelemetryRing::ack(uint32_t seq)
{
    const uint32_t upto = seq + 1;
    if (upto > acked_upto_) acked_upto_ = upto > next_seq_ ? next_seq_ : upto;
}
