#pragma once

#include <stdint.h>

#include "CanTypes.h"

class ObdPidCatalog {
public:
    ObdPidCatalog();

    void reset();
    bool accept(const CanFrame &frame);
    bool supports(uint8_t pid) const;
    uint8_t next_query_base() const;
    bool complete() const;

private:
    bool supported_[256];
    uint8_t next_query_base_;
    bool complete_;
};

extern ObdPidCatalog obd_pid_catalog;
