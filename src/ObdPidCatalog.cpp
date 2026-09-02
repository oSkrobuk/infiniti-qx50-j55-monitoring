#include "ObdPidCatalog.h"

#include <string.h>

ObdPidCatalog obd_pid_catalog;

ObdPidCatalog::ObdPidCatalog()
{
    reset();
}

void ObdPidCatalog::reset()
{
    memset(supported_, 0, sizeof(supported_));
    next_query_base_ = 0;
    complete_ = false;
}

bool ObdPidCatalog::accept(const CanFrame &frame)
{
    if (frame.id < 0x7E8 || frame.id > 0x7EF || frame.dlc < 7 || frame.data[1] != 0x41) {
        return false;
    }

    const uint8_t base = frame.data[2];
    if (base != next_query_base_ || (base & 0x1F) != 0) {
        return false;
    }

    for (uint8_t offset = 0; offset < 32; ++offset) {
        const uint16_t pid = static_cast<uint16_t>(base) + offset + 1;
        if (pid > 0xFF) break;
        const uint8_t byte_index = static_cast<uint8_t>(3 + (offset / 8));
        const uint8_t bit = static_cast<uint8_t>(0x80 >> (offset % 8));
        supported_[pid] = (frame.data[byte_index] & bit) != 0;
    }

    const uint16_t next = static_cast<uint16_t>(base) + 32;
    if (next <= 0xE0 && supported_[next]) {
        next_query_base_ = static_cast<uint8_t>(next);
    } else {
        next_query_base_ = 0xFF;
        complete_ = true;
    }
    return true;
}

bool ObdPidCatalog::supports(uint8_t pid) const
{
    return supported_[pid];
}

uint8_t ObdPidCatalog::next_query_base() const
{
    return next_query_base_;
}

bool ObdPidCatalog::complete() const
{
    return complete_;
}
