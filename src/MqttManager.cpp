#include "MqttManager.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "NetworkCatalog.h"

MqttManager& MqttManager::instance()
{
    static MqttManager inst;
    return inst;
}

void MqttManager::begin(const MqttSettings& settings)
{
    _settings = settings;

    if (_settings.clientId.length() == 0)
    {
        char idBuf[32];
        snprintf(idBuf, sizeof(idBuf),
                 "esp32-%012llX",
                 ESP.getEfuseMac());
        _settings.clientId = idBuf;
    }

    _client.setURL(
        _settings.host.c_str(),
        _settings.port,
        _settings.username.c_str(),
        _settings.password.c_str()
    );

    _client.setMqttClientName(_settings.clientId.c_str());
    _client.setKeepAlive(30);
    _client.setAutoReconnect(true);

    _client.setOnMessageCallback([this](const std::string& topic, const std::string& payload) {
        this->handleIncomingMessage(topic, payload);
    });

    Serial.println("[MQTT] Starting connection...");
    _client.loopStart();
}

bool MqttManager::isConnected() const
{
    return _client.isConnected();
}

bool MqttManager::publish(const String& topic,
                          const String& payload,
                          uint8_t qos,
                          bool retain)
{
    if (!_client.isConnected()) return false;

    return _client.publish(
        std::string(topic.c_str()),
        std::string(payload.c_str()),
        qos,
        retain
    );
}

bool MqttManager::publishStatus(const String& payload)
{
    return publish(_settings.publishTopic, payload, _settings.qos);
}

void MqttManager::handleBrokerConnected(esp_mqtt_client_handle_t client)
{
    Serial.println("[MQTT] Connected to broker");

    if (_settings.subscribeTopic.length() > 0)
    {
        _client.subscribe(
            std::string(_settings.subscribeTopic.c_str()),
            [this](const std::string& topic, const std::string& payload)
            {
                handleIncomingMessage(topic, payload);
            },
            _settings.qos
        );
    }
}

void MqttManager::handleIncomingMessage(const std::string& topic, const std::string& payload)
{
    Serial.println("[MQTT IN]");
    Serial.printf("Topic: %s\n", topic.c_str());
    Serial.printf("Payload: %s\n", payload.c_str());

    String t = String(topic.c_str());
    String p = String(payload.c_str());

    if (t.endsWith("/config/wifi")) 
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, p);
        if (!error) {
            String newSsid = doc["ssid"];
            String newPass = doc["password"];
            
            if (NetworkCatalog::isAllowedSSID(newSsid)) {
                Serial.println("[MQTT] Received valid new Wi-Fi credentials. Saving...");
                ConfigStore::saveWifi(newSsid, newPass);
                Serial.println("[MQTT] Restarting device to apply new Wi-Fi networks...");
                delay(1000);
                ESP.restart();
            } else {
                Serial.println("[MQTT] SSID rejected by NetworkCatalog validation rules.");
            }
        }
    }
    else if (t.endsWith("/config/mqtt"))
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, p);
        if (!error) {
            const char* command = doc["command"];
            if (String(command) == "REBOOT") {
                Serial.println("[MQTT] Executing remote UI reboot...");
                delay(1000);
                ESP.restart();
            }
        }
    }
}

ESP32MQTTClient& MqttManager::raw()
{
    return _client;
}

// global callback for MQTT connection event
void onMqttConnect(esp_mqtt_client_handle_t client) 
{
    if (MqttManager::instance().raw().isMyTurn(client)) 
    {
        MqttManager::instance().handleBrokerConnected(client);
    }
}

// Matches the exact library macro signature perfectly
esp_err_t handleMQTT(esp_mqtt_event_handle_t event) 
{
    MqttManager::instance().raw().onEventCallback(event);
    return ESP_OK;
}

//old version of the callback

// void handleMQTT(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
// {
//     auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
//     MqttManager::instance().raw().onEventCallback(event);
// }