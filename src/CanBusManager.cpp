#include "CanBusManager.h"

// Глобальный объект
CanBusManager canBus;

CanBusManager::CanBusManager()
    : _running(false)
    , _rxCount(0)
    , _errCount(0)
    , _callback(nullptr)
{
}

bool CanBusManager::init()
{
    Serial.println("[CAN] Инициализация TWAI (SN65HVD230)...");

    // Конфигурация пинов
    twai_general_config_t gConfig = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
    gConfig.rx_queue_len = 32; // Увеличенная очередь для плотного трафика шины

    // Скоростная конфигурация (500 кбит/с)
    twai_timing_config_t tConfig = CAN_TIMING;

    // Фильтр: принимаем все фреймы
    twai_filter_config_t fConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Установка драйвера
    esp_err_t err = twai_driver_install(&gConfig, &tConfig, &fConfig);
    if (err != ESP_OK)
    {
        Serial.printf("[CAN] Ошибка установки драйвера: %s\n", esp_err_to_name(err));
        return false;
    }

    // Запуск контроллера
    err = twai_start();
    if (err != ESP_OK)
    {
        Serial.printf("[CAN] Ошибка запуска TWAI: %s\n", esp_err_to_name(err));
        twai_driver_uninstall();
        return false;
    }

    _running  = true;
    _rxCount  = 0;
    _errCount = 0;

    Serial.printf("[CAN] Шина запущена. TX=GPIO%d, RX=GPIO%d, 500 кбит/с, режим: только приём\n",
                  (int)CAN_TX_PIN, (int)CAN_RX_PIN);
    return true;
}

void CanBusManager::stop()
{
    if (!_running)
        return;

    twai_stop();
    twai_driver_uninstall();
    _running = false;
    Serial.println("[CAN] Шина остановлена.");
}

void CanBusManager::onFrame(CanFrameCallback cb)
{
    _callback = cb;
}

void CanBusManager::handle()
{
    if (!_running)
        return;

    twai_message_t msg;

    // Читаем все доступные фреймы без блокировки (таймаут = 0)
    while (twai_receive(&msg, 0) == ESP_OK)
    {
        _rxCount++;

        if (_callback)
        {
            CanFrame frame = _toFrame(msg);
            _callback(frame);
        }
    }

    // Проверяем состояние шины и считаем ошибки
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK)
    {
        // Если контроллер попал в Bus-Off — перезапускаем
        if (status.state == TWAI_STATE_BUS_OFF)
        {
            _errCount++;
            Serial.println("[CAN] Bus-Off! Попытка восстановления...");
            twai_initiate_recovery();
        }
    }
}

uint32_t CanBusManager::receivedCount() const
{
    return _rxCount;
}

uint32_t CanBusManager::errorCount() const
{
    return _errCount;
}

bool CanBusManager::isRunning() const
{
    return _running;
}

CanFrame CanBusManager::_toFrame(const twai_message_t &msg)
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
