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
                    case 0x1201: { // Обороты двигателя (Engine RPM)
                        uint16_t raw_rpm = (static_cast<uint16_t>(d[4]) << 8) | d[5];
                        can_metrics.engine_rpm    = static_cast<float>(raw_rpm) * 12.5f;
                        can_metrics.engine_rpm_ts = millis();
                        break;
                    }
                    case 0x110E: { // Датчик усиления турбины
                        // 79 / 50.0f = 1.58 Вольт
                        can_metrics.turbo_boost_volt    = static_cast<float>(d[4]) / 50.0f;
                        can_metrics.turbo_boost_volt_ts = millis();
                        break;
                    }  
                    case 0x1278: { // Датчик давления малса ДВС
                        // Собираем 16-битное значение из d[4] и d[5] (например, 0x014E = 334)
                        uint16_t raw_oil_press = (static_cast<uint16_t>(d[4]) << 8) | d[5];
                        // Переводим в чистые Вольты (334 / 200.0f = 1.67V)
                        can_metrics.oil_pressure_volt    = static_cast<float>(raw_oil_press) / 200.0f;
                        can_metrics.oil_pressure_volt_ts = millis();
                        break;
                    }                               
                    case 0x1103: { // Нарпяжение бортовой сети
                        // 178 * 0.08f = 14.24 Вольт
                        can_metrics.battery_voltage    = static_cast<float>(d[4]) * 0.08f;
                        can_metrics.battery_voltage_ts = millis();
                        break;
                    }
                    default:
                        break;
                }
            }
            break;
        }
        case 0x7E9: {
            // Диагностический ответ от TCM (Блок управления вариатором)
            // Нам физически необходимы минимум 5 байт (индексы d[0]...d[4])
            if (dlc < 5) break;

            // d[1] = 0x62 (Положительный ответ на чтение параметров Service 0x22)
            if (d[1] == 0x62) {
                // Собираем идентификатор параметра DID из байт 2 и 3 (big-endian)
                uint16_t did = (static_cast<uint16_t>(d[2]) << 8) | d[3];

                switch (did) {
                    case 0x110C: { // Температура масла в вариаторе (CVT Fluid Temp)
                        /*
                         * ФИЗИЧЕСКИЙ СМЫСЛ И КАЛИБРОВКА ДЛЯ INFINITI QX50 J55:
                         * В байте d[4] прилетает сырое значение (например, 0x66 = 102 DEC).
                         * Математика блока Jatco CVT8 HT (JF019E) использует смещение -40:
                         * 102 - 40.0f = 62.0°C (точно совпадает с показаниями сканера Launch).
                         * 
                         * СПРАВОЧНЫЕ ДИАПАЗОНЫ:
                         * < 50°C   - Вариатор не прогрет (масло NS-3 густое, нежелательно нагружать).
                         * 70-90°C  - Идеальная рабочая температура для долгой жизни цепи и конусов.
                         * > 100°C  - Повышенный износ, включается скрытый счетчик деградации масла.
                         * > 115°C  - Критический перегрев, аварийный режим защиты трансмиссии.
                         */
                        can_metrics.cvt_temp    = static_cast<float>(static_cast<int16_t>(d[4]) - 40);
                        can_metrics.cvt_temp_ts = millis();
                        break;
                    }
                    case 0x111F: { // Текущая виртуальная передача вариатора
                        // Примечание: На P и N всегда возвращает 1 (превентивный выбор 1-й передачи)
                        can_metrics.cvt_gear    = static_cast<int8_t>(d[4]);
                        can_metrics.cvt_gear_ts = millis();
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
