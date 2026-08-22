#pragma once
#include <Arduino.h>
#include <ESP32MQTTClient.h>
#include "ConfigStore.h"

class MqttManager
{
public:
    static MqttManager& instance();

    void begin(const MqttSettings& settings);
    void loop(); //creates a background task that manages MQTT automatically.

    bool isConnected() const;

    bool publish(const String& topic,
                 const String& payload,
                 uint8_t qos = 2,
                 bool retain = false);

    bool publishStatus(const String& payload);

private:
    MqttManager() = default;

    void handleIncomingMessage(const std::string& topic,
                               const std::string& payload);

public:
    void handleBrokerConnected(esp_mqtt_client_handle_t client);
    ESP32MQTTClient& raw();

private:
    ESP32MQTTClient _client;
    MqttSettings _settings;
};