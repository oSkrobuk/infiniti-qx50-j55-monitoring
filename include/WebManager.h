#pragma once
#include <Arduino.h>
#include <WebServer.h>

class WebManager
{
public:
    // ssid/password — имя и пароль точки доступа ESP32
    WebManager(const char *ssid, const char *password);

    // Запустить WiFi AP и HTTP сервер
    void begin();

    // Вызывать в loop() для обработки входящих запросов
    void handle();

    // IP адрес точки доступа (всегда 192.168.4.1)
    String getIP() const;

private:
    const char *_ssid;
    const char *_password;
    WebServer   _server;

    void handleRoot();
    void handleGetConfig();
    void handlePostConfig();
    void handleNotFound();
};
