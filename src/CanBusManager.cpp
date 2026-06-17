#include "CanBusManager.h"

#include <math.h>

// Глобальный объект шины CAN
CanBusManager can_bus;

// Глобальные метрики CAN — все поля по умолчанию равны 0 (нет данных)
CanMetrics can_metrics = {};

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
        case 0x7E8: {
            // Диагностический ответ от ECM (UDS / ISO 14229)
            // Нам физически необходимы минимум 6 байт (индексы d[0]...d[5])
            if (dlc < 6) break; 
            
            // d[1] = 0x62 (Положительный ответ на чтение параметров Service 0x22)
            if (d[1] == 0x62) { 
                // Собираем идентификатор параметра DID из байт 2 и 3 (big-endian)
                uint16_t did = (static_cast<uint16_t>(d[2]) << 8) | d[3];

                switch (did) {
                    // --- ОДНОБАЙТОВЫЕ ПАРАМЕТРЫ (значение лежит в d[4]) ---
                    case 0x1101: { // Температура ОЖ ДВС
                        can_metrics.engine_coolant    = static_cast<float>(static_cast<int16_t>(d[4]) - 50);
                        can_metrics.engine_coolant_ts = millis();
                        break;
                    }
                    case 0x111F: { // Температура масла ДВС
                        can_metrics.engine_oil    = static_cast<float>(static_cast<int16_t>(d[4]) - 50);
                        can_metrics.engine_oil_ts = millis();
                        break;
                    }
                    case 0x116B: { // Температура ОЖ радиатора
                        can_metrics.radiator_coolant    = static_cast<float>(static_cast<int16_t>(d[4]) - 50);
                        can_metrics.radiator_coolant_ts = millis();
                        break;
                    }

                    // --- ДВУХБАЙТОВЫЕ ПАРАМЕТРЫ (значение собрано из d[4] и d[5]) ---
                    case 0x1104: { // Обороты ДВС
                        uint16_t raw = (static_cast<uint16_t>(d[4]) << 8) | d[5];
                        float rpm = static_cast<float>(raw) / 4.0f;
                        Serial.printf("  >>> [UDS] Обороты: %.0f RPM\n", rpm);
                        break;
                    }
                    case 0x1224: { // Давление наддува
                        uint16_t raw = (static_cast<uint16_t>(d[4]) << 8) | d[5];
                        float abs_bar = static_cast<float>(raw) / 1000.0f;
                        float boost = abs_bar - 1.013f;
                        Serial.printf("  >>> [UDS] Наддув: %.2f bar (Абс: %.2f)\n", boost, abs_bar);
                        break;
                    }
                    case 0x1103: { // ДМРВ / Расход воздуха
                        uint16_t raw = (static_cast<uint16_t>(d[4]) << 8) | d[5];
                        float maf = static_cast<float>(raw) / 100.0f;
                        Serial.printf("  >>> [UDS] Расход воздуха: %.2f g/s\n", maf);
                        break;
                    }
                    case 0x123A: { // Давление масла двигателя
                        uint16_t raw = (static_cast<uint16_t>(d[4]) << 8) | d[5];
                        float oil_press_bar = static_cast<float>(raw) / 100.0f;
                        Serial.printf("  >>> [UDS] Давление масла: %.2f bar\n", oil_press_bar);
                        break;
                    }
                    default:
                        break;
                }
            }
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
    /*const uint8_t *d   = frame.data;
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
    }*/

    // --- Декодирование известных фреймов QX50 J55 ---
    can_parse_known_frames(frame);

    Serial.println();
}
