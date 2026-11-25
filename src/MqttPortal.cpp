#include "MqttPortal.h"

#include <WiFi.h>

#include "ConfigStore.h"
#include "PortalHtml.h"

void MqttPortal::begin()
{
    _server.on("/", HTTP_GET, [this]() {
        _server.sendHeader("Location", "/mqtt");
        _server.send(302, "text/plain", "Redirecting to MQTT configuration");
    });
    _server.on("/mqtt", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save-mqtt", HTTP_POST, [this]() { handleSave(); });
    _server.begin();

    Serial.println("[MQTT Portal] Open http://");
    Serial.println(WiFi.localIP());
}

void MqttPortal::handle()
{
    _server.handleClient();
}

void MqttPortal::handleRoot()
{
    _server.send(200, "text/html", kPortalHtml);
}

void MqttPortal::handleSave()
{
    if (!_server.hasArg("host") || !_server.hasArg("port"))
    {
        _server.send(400, "text/plain", "Broker host and port are required");
        return;
    }

    MqttSettings settings;
    settings.host = _server.arg("host");
    settings.port = static_cast<uint16_t>(_server.arg("port").toInt());
    settings.clientId = _server.hasArg("client_id") ? _server.arg("client_id") : "";
    settings.username = _server.hasArg("username") ? _server.arg("username") : "";
    settings.password = _server.hasArg("password") ? _server.arg("password") : "";
    settings.subscribeTopic = _server.hasArg("subscribe_topic") ? _server.arg("subscribe_topic") : "";
    settings.publishTopic = _server.hasArg("publish_topic") ? _server.arg("publish_topic") : "";

    int qos = _server.hasArg("qos") ? _server.arg("qos").toInt() : 2;
    if (qos < 0 || qos > 2)
        qos = 2;
    settings.qos = static_cast<uint8_t>(qos);

    if (settings.host.length() == 0 || settings.port == 0)
    {
        _server.send(400, "text/plain", "Invalid broker settings");
        return;
    }

    ConfigStore::saveMqtt(settings);
    _server.send(200, "text/html",
                 "<p>MQTT settings saved. Connecting...</p>");
    delay(500);
    ESP.restart();
}
