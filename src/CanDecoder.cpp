#include "CanTypes.h"

#include <Arduino.h>

// Глобальные метрики CAN — все поля по умолчанию равны 0 (нет данных)
CanMetrics can_metrics = {};

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
                    case 0x1278: { // Датчик давления масла ДВС
                        // Собираем 16-битное значение из d[4] и d[5] (например, 0x014E = 334)
                        uint16_t raw_oil_press = (static_cast<uint16_t>(d[4]) << 8) | d[5];
                        // Переводим в чистые Вольты (334 / 200.0f = 1.67V)
                        can_metrics.oil_pressure_volt    = static_cast<float>(raw_oil_press) / 200.0f;
                        can_metrics.oil_pressure_volt_ts = millis();
                        break;
                    }
                    case 0x1103: { // Напряжение бортовой сети
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
