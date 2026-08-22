#pragma once
#include <Arduino.h>

struct WifiCredentials
{
    String ssid;
    String password;
    bool valid = false;
};

struct MqttSettings
{
    String host;
    uint16_t port = 1883;
    String clientId;
    String username;
    String password;
    String subscribeTopic;
    String publishTopic;
    uint8_t qos = 2;
    bool valid = false;
};

class ConfigStore
{
public:
    static WifiCredentials loadWifi();
    static void saveWifi(const String& ssid, const String& password);
    static void clearWifi();
    static bool hasWifiCredentials();

    static MqttSettings loadMqtt();
    static void saveMqtt(const MqttSettings& s);
    static void clearMqtt();
    static bool hasMqttSettings();

    static void clearAll();
};