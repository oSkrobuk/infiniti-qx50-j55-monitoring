#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "CanTypes.h"

// Максимальное количество уникальных записей в истории алертов
static constexpr uint8_t ALERT_LOG_MAX = 16;

// Длительность показа оверлея на дисплее (мс) после срабатывания алерта
static constexpr uint32_t ALERT_DISPLAY_MS = 5000;

// Заводское значение: минимальный интервал между повторными срабатываниями
// одной проверки (мс). Меняется в Web UI, хранится в /checks.json
static constexpr uint32_t ALERT_RETRIGGER_MS_DEF = 15000;

// Заводское значение: сколько мс условие проверки должно держаться, прежде чем
// поднимется алерт. Отсекает кратковременные выбросы показаний за пределы
// диапазона. Меняется в Web UI, хранится в /checks.json
static constexpr uint32_t ALERT_CONFIRM_MS_DEF = 1000;

// Границы, в которые зажимаются тайминги из Web UI (мс).
// Подтверждение можно свести к нулю — тогда алерт срабатывает сразу
static constexpr uint32_t ALERT_CONFIRM_MS_MIN   = 0;
static constexpr uint32_t ALERT_CONFIRM_MS_MAX   = 60000;
static constexpr uint32_t ALERT_RETRIGGER_MS_MIN = 1000;
static constexpr uint32_t ALERT_RETRIGGER_MS_MAX = 3600000;

// Путь к файлу истории алертов (отдельно от конфига)
static constexpr const char *ALERTS_LOG_FILE = "/alerts.json";

// Путь к файлу конфига проверок (тайминги + режим повтора + пороговые значения)
static constexpr const char *CHECKS_CONFIG_FILE = "/checks.json";

// Ключ секции таймингов в /checks.json (рядом с объектами проверок E01..E09)
static constexpr const char *CHECKS_TIMING_KEY = "timing";

// Структура одной записи в журнале сработавших алертов
struct AlertLogEntry {
    char     code[8];         // Код, например "E01"
    char     description[128]; // Описание на русском
    uint32_t count;           // Сколько раз сработала эта проверка
};

// Структура конфигурации одной проверки (хранится в /checks.json).
// Внимание: enabled задает режим повтора, а не включение проверки —
// сама проверка выполняется всегда, отключить ее нельзя
struct CheckConfig {
    bool  enabled; // Режим повтора: true — раз в retrigger_ms, false — однократно, пока код есть в журнале
    float param1;  // Первый порог (смысл зависит от проверки)
    float param2;  // Второй порог (0 если не используется)
    float param3;  // Третий порог (0 если не используется)
};

// Статическое описание одной проверки (не изменяется при работе)
struct CheckDef {
    const char *code;          // Код вида "E01"
    const char *name;          // Краткое имя на английском
    const char *display_name;  // Форматированное имя для дисплея (строки через '\n')
    const char *description;   // Расшифровка на русском (для Web UI и журнала)
    const char *param1_label;  // Метка первого порога (nullptr если не используется)
    const char *param2_label;  // Метка второго порога (nullptr если не используется)
    const char *param3_label;  // Метка третьего порога (nullptr если не используется)
    float       param1_def;    // Значение по умолчанию для param1
    float       param2_def;    // Значение по умолчанию для param2
    float       param3_def;    // Значение по умолчанию для param3
};

class AlertManager {
public:
    AlertManager();

    // Инициализация: монтирует LittleFS (через ConfigManager), загружает конфиги
    bool init();

    // Вызывать раз в 200 мс: проверяет метрики, при срабатывании — алертит
    void update(const CanMetrics &m);

    // Возвращает true, если сейчас активен алерт (в течение ALERT_DISPLAY_MS мс)
    bool has_active_alert() const;

    // Код активного алерта, например "E01" (пустая строка если нет)
    const char *active_code() const;

    // Описание активного алерта (пустая строка если нет)
    const char *active_description() const;

    // Форматированное имя активного алерта для дисплея (строки через '\n')
    const char *active_display_name() const;

    // Текущее время подтверждения условия (мс)
    uint32_t confirm_ms() const { return confirm_ms_; }

    // Текущий интервал повтора алерта (мс)
    uint32_t retrigger_ms() const { return retrigger_ms_; }

    // Сериализовать журнал алертов в JSON-строку для Web API
    String log_to_json() const;

    // Очистить журнал алертов и удалить файл.
    // Заодно снимает запрет на повторный алерт: после очистки любая проверка
    // снова сработает, даже в однократном режиме
    bool clear_log();

    // Сериализовать конфиг проверок (тайминги + пороги) в JSON-строку для Web API
    String checks_to_json() const;

    // Применить конфиг проверок из JSON-строки (из Web UI) и сохранить
    bool checks_from_json(const String &json);

    // Число уникальных записей в журнале
    uint8_t log_count() const { return log_count_; }

    // Статические описания всех проверок — используются в Web UI
    static const CheckDef CHECK_DEFS[];
    static constexpr uint8_t CHECK_COUNT = 9;

private:
    CheckConfig   checks_[CHECK_COUNT];     // Конфиг каждой проверки
    AlertLogEntry log_[ALERT_LOG_MAX];      // Журнал уникальных алертов
    uint8_t       log_count_;               // Текущее число записей в журнале

    uint32_t confirm_ms_;                   // Сколько условие должно держаться до алерта
    uint32_t retrigger_ms_;                 // Интервал повтора алерта в режиме enabled=true

    char     active_code_[8];               // Код активного алерта
    char     active_desc_[128];             // Описание активного алерта (русский)
    char     active_dname_[64];             // Форматированное имя для дисплея (строки через '\n')
    uint32_t active_since_;                 // millis() момента последнего срабатывания

    uint32_t last_trigger_ms_[CHECK_COUNT]; // Таймер антидребезга (режим повтора, enabled=true)
    uint32_t pending_since_[CHECK_COUNT];   // millis() момента, когда условие стало истинным
    bool     pending_active_[CHECK_COUNT];  // true, пока условие проверки нарушено

    // Загрузить конфиг проверок из файла (defaults если файла нет).
    // missing — сюда пишется true, если в файле не хватает проверок или полей
    bool load_checks_(bool &missing);

    // Сохранить конфиг проверок в файл
    bool save_checks_();

    // Загрузить журнал алертов из файла
    bool load_log_();

    // Сохранить журнал алертов в файл
    bool save_log_();

    // Применить значения по умолчанию для всех проверок и таймингов
    void apply_check_defaults_();

    // Найти запись журнала по коду: индекс или -1, если кода в журнале нет
    int16_t find_log_index_(const char *code) const;

    // Выполнить одну проверку: idx — индекс в CHECK_DEFS / checks_
    // triggered — true если условие сейчас нарушено. Алерт поднимается, только
    // когда условие продержалось не меньше confirm_ms_
    void try_trigger_(uint8_t idx, bool triggered);
};

// Глобальный объект — доступен из main.cpp и WebManager
extern AlertManager alert_manager;
