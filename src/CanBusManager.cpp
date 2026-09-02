#include "CanBusManager.h"

#include <math.h>

// Глобальный объект шины CAN
CanBusManager can_bus;

// Метрики can_metrics и декодер can_parse_known_frames() живут в CanDecoder.cpp —
// он не зависит от TWAI и потому собирается под хостом для юнит-тестов

CanBusManager::CanBusManager()
    : driver_installed_(false)
    , running_(false)
    , rx_count_(0)
    , err_count_(0)
    , last_rx_ts_(0)
    , last_ecm_response_ts_(0)
    , last_tcm_response_ts_(0)
    , state_(TWAI_STATE_STOPPED)
    , last_recovery_error_(ESP_OK)
    , callback_(nullptr)
{
}

bool CanBusManager::init()
{
    Serial.println("[CAN] Инициализация TWAI (SN65HVD230)...");

    // В режиме хука контроллер только слушает шину и не подтверждает кадры
#ifdef DID_HOOK_MODE
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
#else
    // Нормальный режим нужен для отправки UDS-запросов
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
#endif
    g_config.rx_queue_len = 128; // Увеличенная очередь для плотного трафика шины
    g_config.tx_queue_len = 16;  // Очередь отправки для UDS-запросов

    // Скоростная конфигурация (500 кбит/с)
    twai_timing_config_t t_config = CAN_TIMING;

    // Фильтр: принимаем все фреймы
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Установка драйвера
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        Serial.printf("[CAN] Ошибка установки драйвера: %s\r\n", esp_err_to_name(err));
        return false;
    }
    driver_installed_ = true;

    // Запуск контроллера
    err = twai_start();
    if (err != ESP_OK) {
        Serial.printf("[CAN] Ошибка запуска TWAI: %s\r\n", esp_err_to_name(err));
        twai_driver_uninstall();
        driver_installed_ = false;
        return false;
    }

    running_   = true;
    state_     = TWAI_STATE_RUNNING;
    rx_count_  = 0;
    err_count_ = 0;
    recovery_.reset();
    recovery_.observe(CanDriverState::RUNNING, millis());
    last_recovery_error_ = ESP_OK;
    last_rx_ts_ = 0;
    last_ecm_response_ts_ = 0;
    last_tcm_response_ts_ = 0;

#ifdef DID_HOOK_MODE
    Serial.printf("[CAN] Шина запущена  TX=GPIO%d, RX=GPIO%d, 500 кбит/с, режим: только прием\r\n",
                  static_cast<int>(CAN_TX_PIN), static_cast<int>(CAN_RX_PIN));
#else
    Serial.printf("[CAN] Шина запущена  TX=GPIO%d, RX=GPIO%d, 500 кбит/с, режим: прием и передача\r\n",
                  static_cast<int>(CAN_TX_PIN), static_cast<int>(CAN_RX_PIN));
#endif
    return true;
}

void CanBusManager::stop()
{
    if (!driver_installed_) return;

    if (running_) twai_stop();
    twai_driver_uninstall();
    driver_installed_ = false;
    running_ = false;
    state_ = TWAI_STATE_STOPPED;
    Serial.println("[CAN] Шина остановлена");
}

void CanBusManager::on_frame(CanFrameCallback cb)
{
    callback_ = cb;
}

bool CanBusManager::send_frame(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    if (!running_) return false;

    twai_message_t msg = {};
    msg.identifier      = id;
    msg.data_length_code = (dlc > 8) ? 8 : dlc;
    msg.flags           = 0; // стандартный 11-битный идентификатор
    memcpy(msg.data, data, msg.data_length_code);

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(5));
    if (err != ESP_OK) {
        Serial.printf("[CAN] send_frame 0x%03X ошибка: %s\r\n", id, esp_err_to_name(err));
        return false;
    }
    return true;
}

void CanBusManager::handle()
{
    if (!driver_installed_) return;

    twai_message_t msg;

    // Читаем все доступные фреймы без блокировки (таймаут = 0)
    while (running_ && twai_receive(&msg, 0) == ESP_OK) {
        rx_count_++;
        last_rx_ts_ = millis();

        if (msg.identifier == 0x7E8) {
            last_ecm_response_ts_ = last_rx_ts_;
        } else if (msg.identifier == 0x7E9) {
            last_tcm_response_ts_ = last_rx_ts_;
        }

        if (callback_) {
            CanFrame frame = to_frame(msg);
            callback_(frame);
        }
    }

    // Проверяем состояние шины и восстанавливаем контроллер после Bus-Off
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
        CanDriverState driver_state = CanDriverState::STOPPED;
        switch (status.state) {
            case TWAI_STATE_RUNNING:
                driver_state = CanDriverState::RUNNING;
                break;
            case TWAI_STATE_BUS_OFF:
                driver_state = CanDriverState::BUS_OFF;
                break;
            case TWAI_STATE_RECOVERING:
                driver_state = CanDriverState::RECOVERING;
                break;
            default:
                break;
        }

        const uint32_t bus_off_before = recovery_.bus_off_count();
        const CanRecoveryAction action = recovery_.observe(driver_state, millis());
        if (recovery_.bus_off_count() != bus_off_before) {
            err_count_++;
            Serial.println("[CAN] Bus-Off, запуск восстановления");
        }

        if (action == CanRecoveryAction::INITIATE) {
            esp_err_t err = twai_initiate_recovery();
            recovery_.complete(action, err == ESP_OK, millis());
            if (err != ESP_OK) {
                last_recovery_error_ = err;
                Serial.printf("[CAN] Ошибка запуска восстановления: %s\r\n", esp_err_to_name(err));
            }
        }

        if (action == CanRecoveryAction::START) {
            esp_err_t err = twai_start();
            recovery_.complete(action, err == ESP_OK, millis());
            if (err == ESP_OK) {
                Serial.println("[CAN] Шина восстановлена и запущена");
            } else {
                last_recovery_error_ = err;
                Serial.printf("[CAN] Ошибка перезапуска после восстановления: %s\r\n", esp_err_to_name(err));
            }
        }

        switch (recovery_.state()) {
            case CanDriverState::RUNNING:
                state_ = TWAI_STATE_RUNNING;
                break;
            case CanDriverState::BUS_OFF:
                state_ = TWAI_STATE_BUS_OFF;
                break;
            case CanDriverState::RECOVERING:
                state_ = TWAI_STATE_RECOVERING;
                break;
            default:
                state_ = TWAI_STATE_STOPPED;
                break;
        }
        running_ = recovery_.state() == CanDriverState::RUNNING;
    }
}

uint32_t CanBusManager::received_count() const
{
    return rx_count_;
}

uint32_t CanBusManager::error_count() const
{
    return err_count_;
}

uint32_t CanBusManager::bus_off_count() const
{
    return recovery_.bus_off_count();
}

uint32_t CanBusManager::recovery_count() const
{
    return recovery_.recovery_count();
}

uint32_t CanBusManager::recovery_attempt_count() const
{
    return recovery_.initiate_attempt_count();
}

uint32_t CanBusManager::recovery_failure_count() const
{
    return recovery_.initiate_failure_count();
}

uint32_t CanBusManager::restart_attempt_count() const
{
    return recovery_.start_attempt_count();
}

uint32_t CanBusManager::restart_failure_count() const
{
    return recovery_.start_failure_count();
}

int32_t CanBusManager::last_recovery_error() const
{
    return last_recovery_error_;
}

uint32_t CanBusManager::last_rx_ts() const
{
    return last_rx_ts_;
}

uint32_t CanBusManager::last_ecm_response_ts() const
{
    return last_ecm_response_ts_;
}

uint32_t CanBusManager::last_tcm_response_ts() const
{
    return last_tcm_response_ts_;
}

const char *CanBusManager::state_name() const
{
    if (!driver_installed_) return "not_installed";

    switch (state_) {
        case TWAI_STATE_STOPPED:
            return "stopped";
        case TWAI_STATE_RUNNING:
            return "running";
        case TWAI_STATE_BUS_OFF:
            return "bus_off";
        case TWAI_STATE_RECOVERING:
            return "recovering";
        default:
            return "unknown";
    }
}

bool CanBusManager::is_running() const
{
    return running_;
}

CanFrame CanBusManager::to_frame(const twai_message_t &msg)
{
    CanFrame f;
    f.id       = msg.identifier;
    f.extended = (msg.flags & TWAI_MSG_FLAG_EXTD) != 0;
    f.rtr      = (msg.flags & TWAI_MSG_FLAG_RTR)  != 0;
    f.dlc      = msg.data_length_code;
    memset(f.data, 0, sizeof(f.data));
    memcpy(f.data, msg.data, f.dlc);
    return f;
}

// -----------------------------------------------------------------------------
// can_print_frame — сырой вывод + декодирование известных фреймов
// -----------------------------------------------------------------------------
void can_print_frame(const CanFrame &frame)
{
    const uint8_t *d = frame.data;

#ifdef USE_MOCK_DATA
    // --- Сырые байты (HEX + DEC) ---
    const uint8_t  dlc = frame.dlc;
    Serial.printf("[CAN] ID=0x%03X DLC=%d HEX:", frame.id, dlc);
    for (uint8_t i = 0; i < dlc; i++) {
        Serial.printf(" %02X", d[i]);
    }
    Serial.print(" | DEC:");
    for (uint8_t i = 0; i < dlc; i++) {
        Serial.printf(" %3d", d[i]);
    }
    Serial.println();
#endif

    // --- Декодирование известных фреймов QX50 J55 ---
    can_parse_known_frames(frame);
}

// -----------------------------------------------------------------------------
// can_print_did_hook_frame — пассивный вывод диагностических ISO-TP кадров
// -----------------------------------------------------------------------------
void can_print_did_hook_frame(const CanFrame &frame)
{
    const bool is_functional_request = frame.id == 0x7DF;
    const bool is_physical_request = frame.id >= 0x7E0 && frame.id <= 0x7E7;
    const bool is_response = frame.id >= 0x7E8 && frame.id <= 0x7EF;
    const bool is_diagnostic = is_functional_request || is_physical_request || is_response;

    const uint8_t dlc = frame.dlc > 8 ? 8 : frame.dlc;
    const uint8_t *d = frame.data;
    const char *direction = is_response ? "RESPONSE" : (is_diagnostic ? "REQUEST" : "CAN");
    const uint8_t pci_type = dlc > 0 ? d[0] >> 4 : 0xFF;
    const char *frame_type = nullptr;
    int8_t service_index = -1;

    if (is_diagnostic && pci_type == 0x0) {
        frame_type = "SINGLE";
        service_index = 1;
    } else if (is_diagnostic && pci_type == 0x1) {
        frame_type = "FIRST";
        service_index = 2;
    } else if (is_diagnostic && pci_type == 0x2) {
        frame_type = "CONSECUTIVE";
    } else if (is_diagnostic && pci_type == 0x3) {
        frame_type = "FLOW_CONTROL";
    }

    Serial.printf("[HOOK] %10lu %s ID=0x%03lX DLC=%u",
                  static_cast<unsigned long>(millis()), direction,
                  static_cast<unsigned long>(frame.id), dlc);

    if (frame_type != nullptr) {
        Serial.printf(" ISO-TP=%s", frame_type);
    }

    if (service_index >= 0 && service_index < dlc) {
        const uint8_t service = d[service_index];
        Serial.printf(" SERVICE=0x%02X", service);

        const bool has_did = service == 0x22 || service == 0x2E || service == 0x2F || service == 0x62 ||
                             service == 0x6E || service == 0x6F;
        if (has_did && service_index + 2 < dlc) {
            const uint16_t did = (static_cast<uint16_t>(d[service_index + 1]) << 8) |
                                 d[service_index + 2];
            Serial.printf(" DID=0x%04X", did);
        } else if (service == 0x7F && service_index + 2 < dlc) {
            Serial.printf(" REJECTED_SERVICE=0x%02X NRC=0x%02X",
                          d[service_index + 1], d[service_index + 2]);
        }
    }

    Serial.print(" HEX=");
    for (uint8_t i = 0; i < dlc; ++i) {
        if (i > 0) Serial.print(' ');
        Serial.printf("%02X", d[i]);
    }

    Serial.print(" DEC=");
    for (uint8_t i = 0; i < dlc; ++i) {
        if (i > 0) Serial.print(' ');
        Serial.printf("%u", d[i]);
    }
    Serial.println();
}
