#include "CanBusManager.h"

#include <math.h>

// Глобальный объект шины CAN
CanBusManager can_bus;

// Метрики can_metrics и декодер can_parse_known_frames() живут в CanDecoder.cpp —
// он не зависит от TWAI и потому собирается под хостом для юнит-тестов

CanBusManager::CanBusManager()
    : running_(false)
    , rx_count_(0)
    , err_count_(0)
    , callback_(nullptr)
{
}

bool CanBusManager::init()
{
    Serial.println("[CAN] Инициализация TWAI (SN65HVD230)...");

    // Конфигурация пинов (нормальный режим — нужен для отправки UDS-запросов)
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
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

    // Запуск контроллера
    err = twai_start();
    if (err != ESP_OK) {
        Serial.printf("[CAN] Ошибка запуска TWAI: %s\r\n", esp_err_to_name(err));
        twai_driver_uninstall();
        return false;
    }

    running_   = true;
    rx_count_  = 0;
    err_count_ = 0;

    Serial.printf("[CAN] Шина запущена. TX=GPIO%d, RX=GPIO%d, 500 кбит/с, режим: приём и передача\r\n",
                  static_cast<int>(CAN_TX_PIN), static_cast<int>(CAN_RX_PIN));
    return true;
}

void CanBusManager::stop()
{
    if (!running_) return;

    twai_stop();
    twai_driver_uninstall();
    running_ = false;
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
    if (!running_) return;

    twai_message_t msg;

    // Читаем все доступные фреймы без блокировки (таймаут = 0)
    while (twai_receive(&msg, 0) == ESP_OK) {
        rx_count_++;

        if (callback_) {
            CanFrame frame = to_frame(msg);
            callback_(frame);
        }
    }

    // Проверяем состояние шины и считаем ошибки
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
        // Если контроллер попал в Bus-Off — перезапускаем
        if (status.state == TWAI_STATE_BUS_OFF) {
            err_count_++;
            Serial.println("[CAN] Bus-Off! Попытка восстановления...");
            twai_initiate_recovery();
        }
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
