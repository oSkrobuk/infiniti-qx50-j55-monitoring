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
            // Положительный ответ OBD-II Mode 01
            if (dlc >= 4 && d[1] == 0x41) {
                const uint8_t pid = d[2];
                const uint32_t now = millis();
                const uint16_t raw = dlc >= 5
                    ? (static_cast<uint16_t>(d[3]) << 8) | d[4]
                    : 0;
                uint32_t generic_raw = 0;
                const uint8_t data_length = d[0] >= 2 ? d[0] - 2 : 0;
                for (uint8_t offset = 0; offset < data_length && offset < 4 && 3 + offset < dlc; ++offset) {
                    generic_raw = (generic_raw << 8) | d[3 + offset];
                }
                float obd_value = 0.0f;
                switch (pid) {
                    case 0x04: obd_value = d[3] * 100.0f / 255.0f; break;
                    case 0x05:
                        can_metrics.engine_coolant = static_cast<float>(d[3]) - 40.0f;
                        can_metrics.engine_coolant_ts = now;
                        can_metrics.engine_coolant_source = MetricSource::OBD2;
                        return;
                    case 0x06:
                    case 0x07: obd_value = d[3] * 100.0f / 128.0f - 100.0f; break;
                    case 0x0A: obd_value = d[3] * 3.0f; break;
                    case 0x0B: obd_value = d[3]; break;
                    case 0x0C:
                        if (dlc < 5) return;
                        can_metrics.engine_rpm = static_cast<float>(raw) / 4.0f;
                        can_metrics.engine_rpm_ts = now;
                        can_metrics.engine_rpm_source = MetricSource::OBD2;
                        return;
                    case 0x0D: obd_value = d[3]; break;
                    case 0x0E: obd_value = d[3] / 2.0f - 64.0f; break;
                    case 0x0F: obd_value = static_cast<float>(d[3]) - 40.0f; break;
                    case 0x10:
                        if (dlc < 5) return;
                        obd_value = raw / 100.0f;
                        break;
                    case 0x11: obd_value = d[3] * 100.0f / 255.0f; break;
                    case 0x1F:
                        if (dlc < 5) return;
                        obd_value = raw;
                        break;
                    case 0x23:
                        if (dlc < 5) return;
                        obd_value = raw * 10.0f;
                        break;
                    case 0x24:
                    case 0x25:
                    case 0x26:
                    case 0x27:
                    case 0x44:
                        if (dlc < 5) return;
                        obd_value = raw * 2.0f / 65536.0f;
                        break;
                    case 0x2F: obd_value = d[3] * 100.0f / 255.0f; break;
                    case 0x33: obd_value = d[3]; break;
                    case 0x3C:
                    case 0x3D:
                        if (dlc < 5) return;
                        obd_value = raw / 10.0f - 40.0f;
                        break;
                    case 0x42:
                        if (dlc < 5) return;
                        can_metrics.battery_voltage = static_cast<float>(raw) / 1000.0f;
                        can_metrics.battery_voltage_ts = now;
                        can_metrics.battery_voltage_source = MetricSource::OBD2;
                        return;
                    case 0x43:
                        if (dlc < 5) return;
                        obd_value = raw * 100.0f / 255.0f;
                        break;
                    case 0x46: obd_value = static_cast<float>(d[3]) - 40.0f; break;
                    case 0x49:
                    case 0x4A:
                    case 0x4B:
                    case 0x4C: obd_value = d[3] * 100.0f / 255.0f; break;
                    case 0x5C:
                        can_metrics.engine_oil = static_cast<float>(d[3]) - 40.0f;
                        can_metrics.engine_oil_ts = now;
                        can_metrics.engine_oil_source = MetricSource::OBD2;
                        return;
                    case 0x5E:
                        if (dlc < 5) return;
                        obd_value = raw / 20.0f;
                        break;
                    case 0x61:
                    case 0x62:
                    case 0x64: obd_value = static_cast<float>(d[3]) - 125.0f; break;
                    case 0x63:
                        if (dlc < 5) return;
                        obd_value = raw;
                        break;
                    default: obd_value = static_cast<float>(generic_raw); break;
                }
                can_metrics.obd[pid] = {obd_value, now};
                return;
            }

            // Диагностический ответ от ECM (UDS / ISO 14229)
            if (dlc < 6) break;

            // d[1] = 0x62 (Положительный ответ на чтение параметров Service 0x22)
            if (d[1] == 0x62) {
                // Собираем идентификатор параметра DID из байт 2 и 3 (big-endian)
                uint16_t did = (static_cast<uint16_t>(d[2]) << 8) | d[3];

                switch (did) {
                    case 0x1101: { // Температура ОЖ ДВС
                        can_metrics.engine_coolant    = static_cast<float>(static_cast<int16_t>(d[4]) - 50);
                        can_metrics.engine_coolant_ts = millis();
                        can_metrics.engine_coolant_source = MetricSource::INFINITI_UDS;
                        break;
                    }
                    case 0x111F: { // Температура масла ДВС
                        can_metrics.engine_oil    = static_cast<float>(static_cast<int16_t>(d[4]) - 50);
                        can_metrics.engine_oil_ts = millis();
                        can_metrics.engine_oil_source = MetricSource::INFINITI_UDS;
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
                        can_metrics.engine_rpm_source = MetricSource::INFINITI_UDS;
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
                        can_metrics.battery_voltage_source = MetricSource::INFINITI_UDS;
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
        case 0x763: {
            // Первый ISO-TP кадр ответа блока состояния освещения на DID 0x0E07
            if (dlc < 7 || (d[0] & 0xF0) != 0x10) break;

            const bool is_light_status = d[2] == 0x62 && d[3] == 0x0E && d[4] == 0x07;
            if (is_light_status) {
                // Второй байт данных DID: 0x0C = OFF, 0x1C = ON
                can_metrics.exterior_light_on = (d[6] & 0x10) != 0;
                can_metrics.exterior_light_ts = millis();
            }
            break;
        }
        default:
            break;
    }
}
