#include <Arduino.h>
#include <WiFi.h>

#include "ConfigStore.h"
#include "NetworkCatalog.h"
#include "MqttManager.h"
#include "WiFiPortal.h"
#include "MqttPortal.h"

constexpr uint32_t kPublishInterval = 5000; //ESP32 will publish MQTT status every 5 seconds

unsigned long lastPublish = 0;
WiFiPortal wifiPortal;
MqttPortal mqttPortal;
bool wifiPortalRunning = false;
bool mqttPortalRunning = false;

bool connectWiFi()
{
    WifiCredentials creds = ConfigStore::loadWifi();

    if (!creds.valid)
    {
        Serial.println("[WiFi] No stored WiFi credentials");
        return false;
    }

    WiFi.mode(WIFI_STA); //esp32 station mode
    WiFi.begin(creds.ssid.c_str(), creds.password.c_str());

    Serial.print("[WiFi] Connecting");

    for (uint8_t attempt = 0;
         attempt < 40 && WiFi.status() != WL_CONNECTED;
         attempt++)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("\n[WiFi] Connection failed");
        return false;
    }

    Serial.println("\n[WiFi] Connected");
    Serial.println(WiFi.localIP());
    return true;
}

void resetNvsIfBootHeld()
{
    pinMode(GPIO_NUM_0, INPUT_PULLUP);

    if (digitalRead(GPIO_NUM_0) == LOW)
    {
        Serial.println("[BOOT] Hold detected, waiting 3 seconds...");
        delay(3000);

        if (digitalRead(GPIO_NUM_0) == LOW)
        {
            Serial.println("[CONFIG] Clearing WiFi and MQTT settings");
            ConfigStore::clearAll();
            delay(500);
            ESP.restart();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    resetNvsIfBootHeld();

    if (!ConfigStore::hasWifiCredentials())
    {
        wifiPortalRunning = true;
        wifiPortal.begin();
        return;
    }

    if (!connectWiFi())
    {
        wifiPortalRunning = true;
        wifiPortal.begin();
        return;
    }

    if (!ConfigStore::hasMqttSettings()) {
        mqttPortalRunning = true;
        mqttPortal.begin();
        return;
    }

    auto mqtt = ConfigStore::loadMqtt();
    MqttManager::instance().begin(mqtt);
}

void loop()
{
    if (wifiPortalRunning)
    {
        wifiPortal.handle();
        return;
    }

    if (mqttPortalRunning)
    {
        mqttPortal.handle();
        return;
    }

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