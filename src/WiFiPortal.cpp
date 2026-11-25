#include "WiFiPortal.h"

#include <WiFi.h>

#include "ConfigStore.h"
#include "PortalHtml.h"

namespace
{
constexpr byte kDnsPort = 53;
constexpr const char* kApSsid = "ESP32-Setup";
}

void WiFiPortal::begin()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid);

    _dnsServer.start(kDnsPort, "*", WiFi.softAPIP());

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/networks", HTTP_GET, [this]() { handleNetworks(); });
    _server.on("/save-wifi", HTTP_POST, [this]() { handleSave(); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();

    Serial.println("[WiFi Portal] Started");
    Serial.println(WiFi.softAPIP());
}

void WiFiPortal::handle()
{
    _dnsServer.processNextRequest();
    _server.handleClient();
}

void WiFiPortal::handleRoot()
{
    _server.send(200, "text/html", kPortalHtml);
}

void WiFiPortal::handleNetworks()
{
    int networkCount = WiFi.scanNetworks();
    String json = "[";

    for (int i = 0; i < networkCount; i++)
    {
        if (i > 0)
            json += ",";

        String ssid = WiFi.SSID(i);
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");
        json += "\"" + ssid + "\"";
    }

    json += "]";
    WiFi.scanDelete();
    _server.send(200, "application/json", json);
}

void WiFiPortal::handleSave()
{
    if (!_server.hasArg("ssid"))
    {
        _server.send(400, "text/plain", "Missing SSID");
        return;
    }

    String ssid = _server.arg("ssid");
    String password = _server.hasArg("password") ? _server.arg("password") : "";

    if (ssid.length() == 0)
    {
        _server.send(400, "text/plain", "SSID is required");
        return;
    }

    ConfigStore::saveWifi(ssid, password);
    _server.send(200, "text/html",
                 "<p>WiFi settings saved. Reconnecting...</p>");
    delay(500);
    ESP.restart();
}

void WiFiPortal::handleNotFound()
{
    handleRoot();
}
