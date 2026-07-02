#include "AlertManager.h"

#include <LittleFS.h>

#include "BuzzerController.h"

// Бузер объявлен в main.cpp — используем extern для доступа без циклических зависимостей
extern BuzzerController buzzer;

// Глобальный объект
AlertManager alert_manager;

// Статические описания всех проверок (не изменяются в рантайме)
// Поля: code, name, display_name, description,
//        param1_label, param2_label, param3_label,
//        param1_def, param2_def, param3_def
const CheckDef AlertManager::CHECK_DEFS[AlertManager::CHECK_COUNT] = {
    {
        "E01",
        "Engine Oil Temp High",
        "ENGINE OIL\nTemperature\nHIGH",
        "Температура масла двигателя превысила максимальный порог",
        "Макс. темп, C", nullptr, nullptr,
        120.0f, 0.0f, 0.0f
    },
    {
        "E02",
        "Engine Coolant Temp High",
        "ENGINE COOLANT\nTemperature\nHIGH",
        "Температура антифриза двигателя превысила максимальный порог",
        "Макс. темп, C", nullptr, nullptr,
        103.0f, 0.0f, 0.0f
    },
    {
        "E03",
        "Radiator Coolant Temp High",
        "RADIATOR\nTemperature\nHIGH",
        "Температура антифриза радиатора превысила максимальный порог",
        "Макс. темп, C", nullptr, nullptr,
        95.0f, 0.0f, 0.0f
    },
    {
        "E04",
        "CVT Oil Temp High",
        "CVT OIL\nTemperature\nHIGH",
        "Температура масла вариатора превысила максимальный порог",
        "Макс. темп, C", nullptr, nullptr,
        110.0f, 0.0f, 0.0f
    },
    {
        "E05",
        "Engine RPM Overspeed",
        "ENGINE\nRPM\nOVERSPEED",
        "Обороты двигателя превысили максимально допустимый предел",
        "Макс. об/мин", nullptr, nullptr,
        6500.0f, 0.0f, 0.0f
    },
    {
        "E06",
        "Battery Voltage Low",
        "BATTERY\nVoltage\nLOW",
        "Напряжение бортовой сети упало ниже минимального порога",
        "Мин. напряж, В", nullptr, nullptr,
        11.5f, 0.0f, 0.0f
    },
    {
        "E07",
        "Battery Voltage High",
        "BATTERY\nVoltage\nHIGH",
        "Напряжение бортовой сети превысило максимальный порог",
        "Макс. напряж, В", nullptr, nullptr,
        15.0f, 0.0f, 0.0f
    },
    {
        "E08",
        "Oil Pressure Low",
        "OIL\nPressure\nLOW",
        "Напряжение датчика давления масла ниже нормы для текущих оборотов",
        "Порог об/мин", "Мин. В (низк. об)", "Мин. В (выс. об)",
        3000.0f, 1.0f, 1.5f
    },
    {
        "E09",
        "Oil-Coolant Temp Delta High",
        "OIL vs COOLANT\nDelta\nHIGH",
        "Разница температур масла и антифриза двигателя превысила допустимый порог",
        "Макс. дельта, C", nullptr, nullptr,
        15.0f, 0.0f, 0.0f
    },
};

// ─────────────────────────────────────────────────────────────────────────────

AlertManager::AlertManager()
    : log_count_(0), active_since_(0)
{
    active_code_[0]  = '\0';
    active_desc_[0]  = '\0';
    active_dname_[0] = '\0';
    apply_check_defaults_();
    memset(last_trigger_ms_, 0, sizeof(last_trigger_ms_));
    memset(shown_once_,      0, sizeof(shown_once_));
    memset(log_, 0, sizeof(log_));
}

void AlertManager::apply_check_defaults_()
{
    for (uint8_t i = 0; i < CHECK_COUNT; ++i) {
        checks_[i].enabled = true;
        checks_[i].param1  = CHECK_DEFS[i].param1_def;
        checks_[i].param2  = CHECK_DEFS[i].param2_def;
        checks_[i].param3  = CHECK_DEFS[i].param3_def;
    }
}

bool AlertManager::init()
{
    // LittleFS уже смонтирован в config.init() — вызываем только загрузку файлов
    load_checks_();
    load_log_();
    Serial.println("[Alert] AlertManager инициализирован");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Основная функция проверки — вызывается каждые 200 мс

void AlertManager::update(const CanMetrics &m)
{
    // E01: температура масла ДВС выше максимума
    if (m.engine_oil_ts != 0) {
        try_trigger_(0, m.engine_oil > checks_[0].param1);
    }

    // E02: температура ОЖ ДВС выше максимума
    if (m.engine_coolant_ts != 0) {
        try_trigger_(1, m.engine_coolant > checks_[1].param1);
    }

    // E03: температура ОЖ радиатора выше максимума
    if (m.radiator_coolant_ts != 0) {
        try_trigger_(2, m.radiator_coolant > checks_[2].param1);
    }

    // E04: температура масла вариатора выше максимума
    if (m.cvt_temp_ts != 0) {
        try_trigger_(3, m.cvt_temp > checks_[3].param1);
    }

    // E05: обороты выше максимума
    if (m.engine_rpm_ts != 0) {
        try_trigger_(4, m.engine_rpm > checks_[4].param1);
    }

    // E06: напряжение бортовой сети ниже минимума
    if (m.battery_voltage_ts != 0) {
        try_trigger_(5, m.battery_voltage < checks_[5].param1);
    }

    // E07: напряжение бортовой сети выше максимума
    if (m.battery_voltage_ts != 0) {
        try_trigger_(6, m.battery_voltage > checks_[6].param1);
    }

    // E08: давление масла (напряжение датчика) ниже нормы для текущих оборотов
    if (m.oil_pressure_volt_ts != 0 && m.engine_rpm_ts != 0) {
        float min_volt = (m.engine_rpm < checks_[7].param1)
            ? checks_[7].param2
            : checks_[7].param3;
        try_trigger_(7, m.oil_pressure_volt < min_volt);
    }

    // E09: разница температур масла и антифриза ДВС выше порога
    if (m.engine_oil_ts != 0 && m.engine_coolant_ts != 0) {
        float delta = m.engine_oil - m.engine_coolant;
        try_trigger_(8, delta > checks_[8].param1);
    }
}

void AlertManager::try_trigger_(uint8_t idx, bool triggered)
{
    if (!triggered) return;

    uint32_t now = millis();

    if (checks_[idx].enabled) {
        // Проверка включена: антидребезг — не повторять чаще ALERT_RETRIGGER_MS
        if (last_trigger_ms_[idx] != 0 &&
            (now - last_trigger_ms_[idx]) < ALERT_RETRIGGER_MS) {
            return;
        }
    } else {
        // Проверка отключена: показываем 1 раз за сессию МК (shown_once_ сбрасывается только при перезагрузке)
        if (shown_once_[idx]) return;
        shown_once_[idx] = true;
    }

    last_trigger_ms_[idx] = now;
    active_since_          = now;
    strncpy(active_code_,  CHECK_DEFS[idx].code,         sizeof(active_code_) - 1);
    strncpy(active_desc_,  CHECK_DEFS[idx].description,  sizeof(active_desc_) - 1);
    strncpy(active_dname_, CHECK_DEFS[idx].display_name, sizeof(active_dname_) - 1);
    active_code_[sizeof(active_code_) - 1]   = '\0';
    active_desc_[sizeof(active_desc_) - 1]   = '\0';
    active_dname_[sizeof(active_dname_) - 1] = '\0';

    // Сигнал бузером
    buzzer.trigger_alert();

    // Логируем: ищем запись с этим кодом
    bool found = false;
    for (uint8_t i = 0; i < log_count_; ++i) {
        if (strncmp(log_[i].code, CHECK_DEFS[idx].code, sizeof(log_[i].code)) == 0) {
            log_[i].count++;
            found = true;
            break;
        }
    }

    if (!found && log_count_ < ALERT_LOG_MAX) {
        strncpy(log_[log_count_].code,        CHECK_DEFS[idx].code,        sizeof(log_[0].code) - 1);
        strncpy(log_[log_count_].description, CHECK_DEFS[idx].description, sizeof(log_[0].description) - 1);
        log_[log_count_].code[sizeof(log_[0].code) - 1]               = '\0';
        log_[log_count_].description[sizeof(log_[0].description) - 1] = '\0';
        log_[log_count_].count = 1;
        log_count_++;
    }

    save_log_();

    Serial.printf("[Alert] Сработала проверка %s: %s\r\n",
                  CHECK_DEFS[idx].code, CHECK_DEFS[idx].description);
}

// ─────────────────────────────────────────────────────────────────────────────
// Запросы состояния активного алерта

bool AlertManager::has_active_alert() const
{
    if (active_code_[0] == '\0') return false;
    return (millis() - active_since_) < ALERT_DISPLAY_MS;
}

const char *AlertManager::active_code() const
{
    return active_code_;
}

const char *AlertManager::active_description() const
{
    return active_desc_;
}

const char *AlertManager::active_display_name() const
{
    return active_dname_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Сериализация для Web API

String AlertManager::log_to_json() const
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < log_count_; ++i) {
        JsonObject obj = arr.add<JsonObject>();
        obj["code"]        = log_[i].code;
        obj["description"] = log_[i].description;
        obj["count"]       = log_[i].count;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

bool AlertManager::clear_log()
{
    memset(log_, 0, sizeof(log_));
    log_count_ = 0;
    active_code_[0] = '\0';
    active_desc_[0] = '\0';

    // Удаляем файл журнала (remove() безопасен если файла нет)
    LittleFS.remove(ALERTS_LOG_FILE);

    Serial.println("[Alert] Журнал алертов очищен");
    return true;
}

String AlertManager::checks_to_json() const
{
    JsonDocument doc;

    for (uint8_t i = 0; i < CHECK_COUNT; ++i) {
        const char *code     = CHECK_DEFS[i].code;
        doc[code]["enabled"] = checks_[i].enabled;
        doc[code]["param1"]  = checks_[i].param1;
        doc[code]["param2"]  = checks_[i].param2;
        doc[code]["param3"]  = checks_[i].param3;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

bool AlertManager::checks_from_json(const String &json)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[Alert] ОШИБКА парсинга JSON проверок: %s\r\n", err.c_str());
        return false;
    }

    for (uint8_t i = 0; i < CHECK_COUNT; ++i) {
        const char *code = CHECK_DEFS[i].code;
        if (doc[code].is<JsonObject>()) {
            JsonObjectConst obj = doc[code].as<JsonObjectConst>();
            if (obj["enabled"].is<bool>()) checks_[i].enabled = obj["enabled"].as<bool>();
            if (obj["param1"].is<float>()) checks_[i].param1  = obj["param1"].as<float>();
            if (obj["param2"].is<float>()) checks_[i].param2  = obj["param2"].as<float>();
            if (obj["param3"].is<float>()) checks_[i].param3  = obj["param3"].as<float>();
        }
    }

    return save_checks_();
}

// ─────────────────────────────────────────────────────────────────────────────
// Персистентность

bool AlertManager::load_checks_()
{
    if (!LittleFS.exists(CHECKS_CONFIG_FILE)) {
        Serial.println("[Alert] Файл проверок не найден, используем значения по умолчанию");
        return save_checks_();
    }

    File f = LittleFS.open(CHECKS_CONFIG_FILE, "r");
    if (!f) {
        Serial.println("[Alert] ОШИБКА: не удалось открыть файл проверок");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Alert] ОШИБКА парсинга файла проверок: %s\r\n", err.c_str());
        return false;
    }

    for (uint8_t i = 0; i < CHECK_COUNT; ++i) {
        const char *code = CHECK_DEFS[i].code;
        if (doc[code].is<JsonObject>()) {
            JsonObjectConst obj = doc[code].as<JsonObjectConst>();
            if (obj["enabled"].is<bool>()) checks_[i].enabled = obj["enabled"].as<bool>();
            if (obj["param1"].is<float>()) checks_[i].param1  = obj["param1"].as<float>();
            if (obj["param2"].is<float>()) checks_[i].param2  = obj["param2"].as<float>();
            if (obj["param3"].is<float>()) checks_[i].param3  = obj["param3"].as<float>();
        }
    }

    Serial.println("[Alert] Конфиг проверок загружен");
    // Перезаписываем файл, чтобы подхватить новые поля и новые проверки
    // (например, always_alert или E09 при обновлении прошивки)
    return save_checks_();
}

bool AlertManager::save_checks_()
{
    File f = LittleFS.open(CHECKS_CONFIG_FILE, "w");
    if (!f) {
        Serial.println("[Alert] ОШИБКА: не удалось открыть файл проверок для записи");
        return false;
    }

    JsonDocument doc;
    for (uint8_t i = 0; i < CHECK_COUNT; ++i) {
        const char *code     = CHECK_DEFS[i].code;
        doc[code]["enabled"] = checks_[i].enabled;
        doc[code]["param1"]  = checks_[i].param1;
        doc[code]["param2"]  = checks_[i].param2;
        doc[code]["param3"]  = checks_[i].param3;
    }

    if (serializeJson(doc, f) == 0) {
        Serial.println("[Alert] ОШИБКА: не удалось записать файл проверок");
        f.close();
        return false;
    }

    f.close();
    Serial.println("[Alert] Конфиг проверок сохранён");
    return true;
}

bool AlertManager::load_log_()
{
    // Если файла нет — это нормальная ситуация (первый запуск или после очистки)
    if (!LittleFS.exists(ALERTS_LOG_FILE)) {
        Serial.println("[Alert] Файл журнала не найден, журнал пуст");
        return true;
    }

    File f = LittleFS.open(ALERTS_LOG_FILE, "r");
    if (!f) {
        // Файл недоступен — считаем журнал пустым, не возвращаем ошибку
        Serial.println("[Alert] Файл журнала недоступен, журнал пуст");
        return true;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        // Повреждённый файл — считаем журнал пустым
        Serial.printf("[Alert] Файл журнала повреждён (%s), журнал пуст\r\n", err.c_str());
        LittleFS.remove(ALERTS_LOG_FILE);
        return true;
    }

    JsonArrayConst arr = doc.as<JsonArrayConst>();
    log_count_ = 0;
    for (JsonObjectConst obj : arr) {
        if (log_count_ >= ALERT_LOG_MAX) break;
        const char *code = obj["code"] | "";
        const char *desc = obj["description"] | "";
        uint32_t count   = obj["count"] | 1u;
        strncpy(log_[log_count_].code,        code, sizeof(log_[0].code) - 1);
        strncpy(log_[log_count_].description, desc, sizeof(log_[0].description) - 1);
        log_[log_count_].code[sizeof(log_[0].code) - 1]               = '\0';
        log_[log_count_].description[sizeof(log_[0].description) - 1] = '\0';
        log_[log_count_].count = count;
        log_count_++;
    }

    Serial.printf("[Alert] Журнал загружен: %u записей\r\n", log_count_);
    return true;
}

bool AlertManager::save_log_()
{
    File f = LittleFS.open(ALERTS_LOG_FILE, "w");
    if (!f) {
        Serial.println("[Alert] ОШИБКА: не удалось открыть файл журнала для записи");
        return false;
    }

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < log_count_; ++i) {
        JsonObject obj = arr.add<JsonObject>();
        obj["code"]        = log_[i].code;
        obj["description"] = log_[i].description;
        obj["count"]       = log_[i].count;
    }

    if (serializeJson(doc, f) == 0) {
        Serial.println("[Alert] ОШИБКА: не удалось записать файл журнала");
        f.close();
        return false;
    }

    f.close();
    return true;
}
