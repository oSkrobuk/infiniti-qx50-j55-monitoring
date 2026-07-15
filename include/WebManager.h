#pragma once
#include <Arduino.h>
#include <WebServer.h>

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
    WebServer server_;

    void handle_root();
    void handle_get_config();
    void handle_post_config();
    void handle_reset();
    void handle_not_found();

    // WiFi: GET /wifi — текущие ssid/password (JSON)
    void handle_get_wifi();
    // WiFi: POST /wifi — сохранить новые ssid/password, перезагрузить устройство
    void handle_post_wifi();

    // OTA: GET /update — страница загрузки прошивки
    void handle_update_page();
    // OTA: POST /update — приём .bin файла (chunked multipart upload)
    void handle_update_upload();

    // Алерты: GET /alerts — журнал сработавших проверок (JSON)
    void handle_get_alerts();
    // Алерты: POST /alerts-clear — очистить журнал
    void handle_clear_alerts();
    // Проверки: GET /checks — конфиг проверок (JSON)
    void handle_get_checks();
    // Проверки: POST /checks — сохранить конфиг проверок (JSON body)
    void handle_post_checks();

    // Метрики: GET /metrics — текущие значения для веб-монитора (JSON)
    void handle_get_metrics();
};
