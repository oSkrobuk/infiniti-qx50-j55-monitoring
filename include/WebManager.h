#pragma once
#include <Arduino.h>
#include <WebServer.h>

#include "OtaImage.h"

// Сколько ждать HTTP-запрос от уже принятого соединения, мс
//
// WebServer обслуживает одно соединение за раз: приняв сокет, он ждет от него
// запрос до HTTP_MAX_DATA_WAIT (5 секунд, задано в WebServer.h) и все это время
// никого больше не принимает. Браузеры при переходе по ссылке открывают
// спекулятивные соединения (preconnect) и часто не шлют в них ничего — сервер
// цепляется за такой молчащий сокет, а реальный запрос ждет в очереди accept.
// Переопределить HTTP_MAX_DATA_WAIT через -D нельзя: в WebServer.h он объявлен
// без #ifndef, поэтому значение из заголовка все равно перебьет флаг сборки.
//
// По локальной сети запрос приходит за единицы миллисекунд, так что трети
// секунды хватает с запасом даже для телефона в энергосбережении
static constexpr uint32_t IDLE_CLIENT_TIMEOUT_MS = 300;

// WebServer, который не дает молчащему соединению занимать сервер.
//
// Перед каждым шагом конечного автомата рвет соединение, которое дольше
// IDLE_CLIENT_TIMEOUT_MS не прислало ни байта. Дальше вмешиваться не нужно:
// закрытый сокет базовый handleClient() уберет сам своей штатной веткой
// очистки, а следующим вызовом примет из очереди настоящий запрос.
//
// Проверка безопасна для загрузки прошивки по OTA: как только в сокете
// появляются данные, handleClient() сразу уходит из HC_WAIT_READ и читает тело
// запроса целиком внутри одного вызова — состояние «принят, но не прислал
// ни байта» за время загрузки не возникает
class FastWebServer : public WebServer {
public:
    explicit FastWebServer(uint16_t port)
        : WebServer(port)
    {
    }

    // Вызывать вместо handleClient()
    void handle_client()
    {
        if (_currentStatus == HC_WAIT_READ &&
            millis() - _statusChange > IDLE_CLIENT_TIMEOUT_MS &&
            !_currentClient.available()) {
            _currentClient.stop();
        }

        handleClient();
    }
};

class WebManager {
public:
    // Конструктор без аргументов — WiFi ssid/password читаются из конфига в begin()
    WebManager();

    // Запустить WiFi AP и HTTP сервер
    void begin();

    // Вызывать в loop() для обработки входящих запросов
    void handle();

    // IP адрес точки доступа (всегда 192.168.4.1)
    String get_ip() const;

private:
    FastWebServer server_;
    OtaTagStream  ota_upload_tag_;
    String        ota_upload_error_;
    bool          ota_upload_ok_;

    void handle_root();
    void handle_get_config();
    void handle_post_config();
    void handle_reset();
    void handle_not_found();

    // Метрики: GET /live — отдельная страница живого просмотра метрик
    void handle_live_page();
    // Метрики: GET /metrics — текущие значения метрик с CAN-шины (JSON)
    void handle_get_metrics();

    // Версия: GET /version — версия прошивки, метка сборки, слот OTA, окружение, аптайм (JSON)
    void handle_get_version();

    // WiFi: GET /wifi — текущие ssid/password (JSON)
    void handle_get_wifi();
    // WiFi: POST /wifi — сохранить новые ssid/password, перезагрузить устройство
    void handle_post_wifi();

    // OTA: GET /update — страница загрузки прошивки
    void handle_update_page();
    // OTA: POST /update — приём .bin файла (chunked multipart upload)
    void handle_update_upload();
    // OTA: GET /slots — что лежит в обоих слотах прошивки (JSON)
    void handle_get_slots();
    // OTA: POST /boot-slot?slot=ota_N — сделать слот загрузочным
    void handle_post_boot_slot();

    // Алерты: GET /alerts — журнал сработавших проверок (JSON)
    void handle_get_alerts();
    // Алерты: POST /alerts-clear — очистить журнал
    void handle_clear_alerts();
    // Проверки: GET /checks — конфиг проверок (JSON)
    void handle_get_checks();
    // Проверки: POST /checks — сохранить конфиг проверок (JSON body)
    void handle_post_checks();
};
