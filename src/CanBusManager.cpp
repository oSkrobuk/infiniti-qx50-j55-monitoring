#include "CanBusManager.h"

#include <math.h>

// Глобальный объект
CanBusManager can_bus;

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

    // Конфигурация пинов
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
    g_config.rx_queue_len = 128; // Увеличенная очередь для плотного трафика шины

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

    Serial.printf("[CAN] Шина запущена. TX=GPIO%d, RX=GPIO%d, 500 кбит/с, режим: только приём\r\n",
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
// can_parse_known_frames — декодирование известных фреймов Infiniti QX50 J55
// -----------------------------------------------------------------------------
void can_parse_known_frames(const CanFrame &frame)
{
    const uint8_t *d   = frame.data;
    const uint8_t  dlc = frame.dlc;

    if (dlc < 1) return;

    switch (frame.id) {
    case 0x1F9: {
        // Обороты двигателя: байты 0-1, big-endian
        if (dlc < 2) break;
        uint16_t raw = (static_cast<uint16_t>(d[0]) << 8) | d[1];
        Serial.printf("  >>> Обороты ДВС: %.0f RPM\n", static_cast<float>(raw));
        break;
    }
    case 0x421: {
        // Передача CVT (младший нибл байта 0) + температура коробки (байт 3, смещение -40)
        if (dlc < 4) break;
        uint8_t gear     = d[0] & 0x0F;
        int16_t cvt_temp = static_cast<int16_t>(d[3]) - 40;
        Serial.printf("  >>> Передача: %d\n", static_cast<int>(gear));
        Serial.printf("  >>> Т коробки: %d °C\n", static_cast<int>(cvt_temp));
        break;
    }
    case 0x551: {
        // Температура охлаждающей жидкости ДВС: байт 0, смещение -56
        int16_t water_temp = static_cast<int16_t>(d[0]) - 56;
        Serial.printf("  >>> Т воды ДВС: %d °C\n", static_cast<int>(water_temp));
        break;
    }
    case 0x558: {
        // Температура масла ДВС: байт 2, смещение -40
        if (dlc < 3) break;
        int16_t oil_temp = static_cast<int16_t>(d[2]) - 40;
        Serial.printf("  >>> Т масла ДВС: %d °C\n", static_cast<int>(oil_temp));
        break;
    }
    case 0x11B: {
        // Давление наддува: байты 0-1, big-endian, масштаб 0.001 bar, минус атмосфера
        if (dlc < 2) break;
        uint16_t raw     = (static_cast<uint16_t>(d[0]) << 8) | d[1];
        float    abs_bar = (raw * 0.1f) / 100.0f;
        float    boost   = abs_bar - 1.013f;
        Serial.printf("  >>> Наддув: %.2f bar\n", boost);
        Serial.printf("  >>> Наддув абс. %.2f bar\n", abs_bar);
        break;
    }
    default:
        break;
    }
}

// -----------------------------------------------------------------------------
// can_print_frame — сырой вывод + декодирование известных фреймов
// -----------------------------------------------------------------------------
void can_print_frame(const CanFrame &frame)
{
    const uint8_t *d   = frame.data;
    const uint8_t  dlc = frame.dlc;

    // --- Сырые байты (HEX + DEC) ---
    Serial.printf("[CAN] ID=0x%03X DLC=%d HEX:", frame.id, dlc);
    for (uint8_t i = 0; i < dlc; i++) {
        Serial.printf(" %02X", d[i]);
    }
    Serial.print(" | DEC:");
    for (uint8_t i = 0; i < dlc; i++) {
        Serial.printf(" %3d", d[i]);
    }
    Serial.println();

    // --- uint16_t / int16_t (big-endian и little-endian) для каждой пары байт ---
    if (dlc >= 2) {
        Serial.printf("           u16 BE:");
        for (uint8_t i = 0; i + 1 < dlc; i++) {
            uint16_t be = (static_cast<uint16_t>(d[i]) << 8) | d[i + 1];
            Serial.printf(" [%d-%d]=%5u", i, i + 1, be);
        }
        Serial.println();

        Serial.printf("           u16 LE:");
        for (uint8_t i = 0; i + 1 < dlc; i++) {
            uint16_t le;
            memcpy(&le, &d[i], 2);
            Serial.printf(" [%d-%d]=%5u", i, i + 1, le);
        }
        Serial.println();

        Serial.printf("           i16 BE:");
        for (uint8_t i = 0; i + 1 < dlc; i++) {
            int16_t be = static_cast<int16_t>(
                (static_cast<uint16_t>(d[i]) << 8) | d[i + 1]);
            Serial.printf(" [%d-%d]=%6d", i, i + 1, static_cast<int>(be));
        }
        Serial.println();
    }

    // --- uint32_t / float (big-endian и little-endian) для каждой четвёрки байт ---
    if (dlc >= 4) {
        Serial.printf("           u32 BE:");
        for (uint8_t i = 0; i + 3 < dlc; i++) {
            uint32_t be = (static_cast<uint32_t>(d[i])   << 24)
                        | (static_cast<uint32_t>(d[i+1]) << 16)
                        | (static_cast<uint32_t>(d[i+2]) <<  8)
                        |  static_cast<uint32_t>(d[i+3]);
            Serial.printf(" [%d-%d]=%10lu", i, i + 3, static_cast<unsigned long>(be));
        }
        Serial.println();

        Serial.printf("           u32 LE:");
        for (uint8_t i = 0; i + 3 < dlc; i++) {
            uint32_t le;
            memcpy(&le, &d[i], 4);
            Serial.printf(" [%d-%d]=%10lu", i, i + 3, static_cast<unsigned long>(le));
        }
        Serial.println();

        Serial.printf("           f32 LE:");
        for (uint8_t i = 0; i + 3 < dlc; i++) {
            float f;
            memcpy(&f, &d[i], 4);
            if (isfinite(f)) {
                Serial.printf(" [%d-%d]=%10.3f", i, i + 3, f);
            } else {
                Serial.printf(" [%d-%d]=      NaN", i, i + 3);
            }
        }
        Serial.println();
    }

    // --- Декодирование известных фреймов QX50 J55 ---
    can_parse_known_frames(frame);

    Serial.println();
}
