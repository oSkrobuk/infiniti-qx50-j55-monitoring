#pragma once

#include <cstddef>
#include <cstdint>

struct MockEsp {
    uint64_t getEfuseMac() const { return 0x123456789ABCu; }
};
inline MockEsp ESP;
inline void esp_fill_random(void *target, size_t size)
{
    auto *bytes = static_cast<uint8_t *>(target);
    for (size_t i = 0; i < size; ++i) bytes[i] = static_cast<uint8_t>(16 + i);
}
