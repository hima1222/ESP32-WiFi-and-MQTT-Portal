#include <Arduino.h>
#include <WiFi.h>

#include "ConfigStore.h"
#include "NetworkCatalog.h"
#include "MqttManager.h"

constexpr uint32_t kPublishInterval = 5000; //ESP32 will publish MQTT status every 5 seconds

unsigned long lastPublish = 0;

void connectWiFi()
{
    WifiCredentials creds = ConfigStore::loadWifi();

    if (!creds.valid)
    {
        Serial.println("[WiFi] No stored WiFi, using default");

        creds.ssid = NetworkCatalog::defaultSSID();
        creds.password = NetworkCatalog::defaultPassword();

        ConfigStore::saveWifi(creds.ssid, creds.password);
    }

    WiFi.mode(WIFI_STA); //esp32 station mode
    WiFi.begin(creds.ssid.c_str(), creds.password.c_str());

    Serial.print("[WiFi] Connecting");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n[WiFi] Connected");
    Serial.println(WiFi.localIP());
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    //ConfigStore::clearAll(); // Clear all stored configurations for testing purposes
    
    connectWiFi();

    // if (!ConfigStore::hasMqttSettings())
    // {
    //     Serial.println("[MQTT] No config found. Waiting for MQTT config...");
    //     return;
    // }

    //If no MQTT configuration exists in flash memory, create a default one and save it
    if (!ConfigStore::hasMqttSettings()) {
        MqttSettings defaultMqtt;
        defaultMqtt.host = "test.mosquitto.org";
        defaultMqtt.port = 1883;
        defaultMqtt.clientId = "esp32-himara-01";
        defaultMqtt.subscribeTopic = "himara/esp32-01/#"; // Listens to all configuration triggers
        defaultMqtt.publishTopic = "himara/esp32-01/status"; // Pushes metrics here
        defaultMqtt.qos = 2;
        ConfigStore::saveMqtt(defaultMqtt);
    }

    auto mqtt = ConfigStore::loadMqtt();
    MqttManager::instance().begin(mqtt);
}

void loop()
{
    auto& mqtt = MqttManager::instance();

    if (mqtt.isConnected() &&
        millis() - lastPublish > kPublishInterval)
    {
        lastPublish = millis();

        String payload = "{\"uptime\":";
        payload += millis();
        payload += ",\"rssi\":";
        payload += WiFi.RSSI();
        payload += "}";

        mqtt.publishStatus(payload);
    }
}