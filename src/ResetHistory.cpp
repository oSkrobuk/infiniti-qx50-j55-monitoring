#include "ResetHistory.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "FsUtils.h"

static constexpr const char *RESET_HISTORY_FILE = "/resets.json";

ResetHistory reset_history;

ResetHistory::ResetHistory()
    : reasons_{}
    , count_(0)
    , current_reason_(ESP_RST_UNKNOWN)
{
}

bool ResetHistory::init()
{
    load_();

    current_reason_ = esp_reset_reason();
    const uint8_t move_count = count_ < RESET_HISTORY_MAX ? count_ : RESET_HISTORY_MAX - 1;
    for (uint8_t i = move_count; i > 0; --i) {
        reasons_[i] = reasons_[i - 1];
    }
    reasons_[0] = current_reason_;
    if (count_ < RESET_HISTORY_MAX) count_++;

    Serial.printf("[Reset] Причина загрузки: %s (%d)\r\n",
                  current_reason_name(), static_cast<int>(current_reason_));
    return save_();
}

esp_reset_reason_t ResetHistory::current_reason() const
{
    return current_reason_;
}

const char *ResetHistory::current_reason_name() const
{
    return reason_name(current_reason_);
}

String ResetHistory::to_json() const
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < count_; ++i) {
        JsonObject obj = arr.add<JsonObject>();
        obj["reason"] = reason_name(reasons_[i]);
        obj["code"] = static_cast<int>(reasons_[i]);
    }

    String json;
    serializeJson(doc, json);
    return json;
}

const char *ResetHistory::reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:
            return "power_on";
        case ESP_RST_EXT:
            return "external";
        case ESP_RST_SW:
            return "software";
        case ESP_RST_PANIC:
            return "panic";
        case ESP_RST_INT_WDT:
            return "interrupt_watchdog";
        case ESP_RST_TASK_WDT:
            return "task_watchdog";
        case ESP_RST_WDT:
            return "watchdog";
        case ESP_RST_DEEPSLEEP:
            return "deep_sleep";
        case ESP_RST_BROWNOUT:
            return "brownout";
        case ESP_RST_SDIO:
            return "sdio";
        case ESP_RST_UNKNOWN:
        default:
            return "unknown";
    }
}

bool ResetHistory::load_()
{
    count_ = 0;
    if (!LittleFS.exists(RESET_HISTORY_FILE)) return true;

    File file = LittleFS.open(RESET_HISTORY_FILE, "r");
    if (!file) {
        Serial.println("[Reset] Не удалось открыть журнал загрузок");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err || !doc.is<JsonArray>()) {
        Serial.println("[Reset] Журнал загрузок поврежден, создаем новый");
        return false;
    }

    for (JsonVariantConst item : doc.as<JsonArrayConst>()) {
        if (count_ >= RESET_HISTORY_MAX || !item.is<int>()) break;
        reasons_[count_++] = static_cast<esp_reset_reason_t>(item.as<int>());
    }
    return true;
}

bool ResetHistory::save_() const
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (uint8_t i = 0; i < count_; ++i) {
        arr.add(static_cast<int>(reasons_[i]));
    }

    if (!fs_write_json_atomic(RESET_HISTORY_FILE, doc)) {
        Serial.println("[Reset] Не удалось сохранить журнал загрузок");
        return false;
    }
    return true;
}
