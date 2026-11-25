#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

class WiFiPortal
{
public:
    void begin();
    void handle();

private:
    DNSServer _dnsServer;
    WebServer _server{80};

    void handleRoot();
    void handleSave();
    void handleNetworks();
    void handleNotFound();
};
