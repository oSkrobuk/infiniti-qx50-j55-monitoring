#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

inline std::map<std::string, std::vector<uint8_t>> mock_nvs;
inline bool mock_nvs_write_failed = false;

class Preferences {
public:
    bool begin(const char *, bool = false) { return true; }
    void end() {}
    bool remove(const char *key)
    {
        if (mock_nvs_write_failed) return false;
        return mock_nvs.erase(key) != 0;
    }
    size_t getBytesLength(const char *key) { return mock_nvs[key].size(); }
    size_t getBytes(const char *key, void *target, size_t capacity)
    {
        const auto &data = mock_nvs[key];
        const size_t size = std::min(capacity, data.size());
        if (size != 0) memcpy(target, data.data(), size);
        return size;
    }
    size_t putBytes(const char *key, const void *source, size_t size)
    {
        if (mock_nvs_write_failed) return 0;
        const auto *bytes = static_cast<const uint8_t *>(source);
        mock_nvs[key] = std::vector<uint8_t>(bytes, bytes + size);
        return size;
    }
    uint32_t getUInt(const char *key, uint32_t fallback)
    {
        uint32_t result = fallback;
        if (getBytesLength(key) == sizeof(result)) getBytes(key, &result, sizeof(result));
        return result;
    }
    size_t putUInt(const char *key, uint32_t value) { return putBytes(key, &value, sizeof(value)); }
    uint8_t getUChar(const char *key, uint8_t fallback)
    {
        uint8_t result = fallback;
        if (getBytesLength(key) == 1) getBytes(key, &result, 1);
        return result;
    }
    size_t putUChar(const char *key, uint8_t value) { return putBytes(key, &value, 1); }
};
