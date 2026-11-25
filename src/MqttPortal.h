#pragma once

#include <Arduino.h>
#include <WebServer.h>

class MqttPortal
{
public:
    void begin();
    void handle();

private:
    WebServer _server{80};

    void handleRoot();
    void handleSave();
};
