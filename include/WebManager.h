#pragma once
#include <Arduino.h>
#include <WebServer.h>

class WebManager {
public:
    // ssid/password — имя и пароль точки доступа ESP32
    WebManager(const char *ssid, const char *password);

    // Запустить WiFi AP и HTTP сервер
    void begin();

    // Вызывать в loop() для обработки входящих запросов
    void handle();

    // IP адрес точки доступа (всегда 192.168.4.1)
    String get_ip() const;

private:
    const char *ssid_;
    const char *password_;
    WebServer   server_;

    void handle_root();
    void handle_get_config();
    void handle_post_config();
    void handle_reset();
    void handle_not_found();
    // OTA: GET /update — страница загрузки прошивки
    void handle_update_page();
    // OTA: POST /update — приём .bin файла (chunked multipart upload)
    void handle_update_upload();

    // Алерты: GET /alerts — журнал сработавших проверок (JSON)
    void handle_get_alerts();
    // Алерты: POST /alerts/clear — очистить журнал
    void handle_clear_alerts();
    // Проверки: GET /checks — конфиг проверок (JSON)
    void handle_get_checks();
    // Проверки: POST /checks — сохранить конфиг проверок (JSON body)
    void handle_post_checks();
};
