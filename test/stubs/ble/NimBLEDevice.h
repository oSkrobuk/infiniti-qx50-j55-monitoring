#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

static constexpr int BLE_HS_ADV_F_DISC_GEN = 2;
static constexpr int BLE_HS_ADV_F_BREDR_UNSUP = 4;
static constexpr int BLE_GAP_CONN_MODE_NON = 0;
static constexpr int BLE_GAP_CONN_MODE_UND = 2;
namespace NIMBLE_PROPERTY {
static constexpr uint32_t READ = 1, WRITE = 2, NOTIFY = 4;
}

struct MockNotification {
    std::string uuid;
    std::vector<uint8_t> bytes;
};
inline std::vector<MockNotification> mock_notifications;
inline uint16_t mock_mtu = 185;

class NimBLECharacteristic;
class NimBLEServer;
class NimBLECharacteristicCallbacks {
public:
    virtual ~NimBLECharacteristicCallbacks() = default;
    virtual void onRead(NimBLECharacteristic *) {}
    virtual void onWrite(NimBLECharacteristic *) {}
};
class NimBLEServerCallbacks {
public:
    virtual ~NimBLEServerCallbacks() = default;
    virtual void onConnect(NimBLEServer *) {}
    virtual void onDisconnect(NimBLEServer *) {}
};
class NimBLECharacteristic {
public:
    std::string uuid;
    uint32_t properties;
    std::vector<uint8_t> value;
    std::unique_ptr<NimBLECharacteristicCallbacks> callbacks;
    NimBLECharacteristic(const char *id, uint32_t flags) : uuid(id), properties(flags) {}
    void setCallbacks(NimBLECharacteristicCallbacks *next) { callbacks.reset(next); }
    void setValue(const uint8_t *bytes, size_t size) { value.assign(bytes, bytes + size); }
    void notify() { mock_notifications.push_back({uuid, value}); }
    std::string getValue() { return std::string(value.begin(), value.end()); }
    std::vector<uint8_t> read()
    {
        if (callbacks) callbacks->onRead(this);
        return value;
    }
    void write(const std::vector<uint8_t> &bytes)
    {
        value = bytes;
        if (callbacks) callbacks->onWrite(this);
    }
};
class NimBLEService {
public:
    std::string uuid;
    std::map<std::string, std::unique_ptr<NimBLECharacteristic>> characteristics;
    NimBLECharacteristic *createCharacteristic(const char *id, uint32_t flags)
    {
        auto item = std::make_unique<NimBLECharacteristic>(id, flags);
        auto *result = item.get();
        characteristics[id] = std::move(item);
        return result;
    }
    void start() {}
};
class NimBLEServer {
public:
    NimBLEService service;
    std::unique_ptr<NimBLEServerCallbacks> callbacks;
    void setCallbacks(NimBLEServerCallbacks *next) { callbacks.reset(next); }
    NimBLEService *createService(const char *id) { service.uuid = id; return &service; }
    std::vector<uint16_t> getPeerDevices() { return {1}; }
    uint16_t getPeerMTU(uint16_t) { return mock_mtu; }
    void disconnect(uint16_t) { if (callbacks) callbacks->onDisconnect(this); }
};
class NimBLEUUID {
public:
    std::string value;
    explicit NimBLEUUID(const char *uuid) : value(uuid) {}
};
class NimBLEAdvertisementData {
public:
    std::string name;
    std::string service;
    std::vector<uint8_t> manufacturer;
    void setFlags(int) {}
    void setCompleteServices(const NimBLEUUID &uuid) { service = uuid.value; }
    void setName(const char *value) { name = value; }
    void setManufacturerData(const std::vector<uint8_t> &bytes) { manufacturer = bytes; }
};
class NimBLEAdvertising {
public:
    NimBLEAdvertisementData advertisement, response;
    bool active = false;
    bool isAdvertising() { return active; }
    void start() { active = true; }
    void stop() { active = false; }
    void setAdvertisementData(const NimBLEAdvertisementData &data) { advertisement = data; }
    void setScanResponseData(const NimBLEAdvertisementData &data) { response = data; }
    void setScanResponse(bool) {}
    void setMinInterval(uint16_t) {}
    void setMaxInterval(uint16_t) {}
    void setAdvertisementType(int) {}
};
class NimBLEDevice {
public:
    static inline NimBLEServer server;
    static inline NimBLEAdvertising advertising;
    static inline std::string name;
    static void init(const char *value) { name = value; }
    static void setMTU(uint16_t) {}
    static NimBLEServer *createServer() { server = NimBLEServer(); return &server; }
    static NimBLEAdvertising *getAdvertising() { return &advertising; }
};
