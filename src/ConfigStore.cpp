#include "ConfigStore.h"
#include <Preferences.h> //ESP32 library that accesses the NVS

namespace //NVS namespace names for storing Wi-Fi and MQTT settings
{
    const char* kWifiNamespace = "wifi_cfg";
    const char* kMqttNamespace = "mqtt_cfg";
}

WifiCredentials ConfigStore::loadWifi()
{
    WifiCredentials creds;
    Preferences prefs;

    prefs.begin(kWifiNamespace, true); //read only mode
    creds.ssid = prefs.getString("ssid", "");
    creds.password = prefs.getString("pass", "");
    prefs.end();

    creds.valid = creds.ssid.length() > 0;
    return creds;
}

void ConfigStore::saveWifi(const String& ssid, const String& password)
{
    Preferences prefs;

    prefs.begin(kWifiNamespace, false); //read/write mode
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
}

void ConfigStore::clearWifi()
{
    Preferences prefs;

    prefs.begin(kWifiNamespace, false);
    prefs.clear();
    prefs.end();
}

bool ConfigStore::hasWifiCredentials()
{
    return loadWifi().valid;
}

//  MQTT

MqttSettings ConfigStore::loadMqtt()
{
    MqttSettings s;
    Preferences prefs;

    prefs.begin(kMqttNamespace, true);
    s.host = prefs.getString("host", "");
    s.port = prefs.getUShort("port", 1883);
    s.clientId = prefs.getString("cid", "");
    s.username = prefs.getString("user", "");
    s.password = prefs.getString("pwd", "");
    s.subscribeTopic = prefs.getString("sub", "");
    s.publishTopic = prefs.getString("pub", "");
    s.qos = prefs.getUChar("qos", 2);
    prefs.end();

    s.valid = s.host.length() > 0;
    return s;
}

void ConfigStore::saveMqtt(const MqttSettings& s)
{
    Preferences prefs;

    prefs.begin(kMqttNamespace, false);
    prefs.putString("host", s.host);
    prefs.putUShort("port", s.port);
    prefs.putString("cid", s.clientId);
    prefs.putString("user", s.username);
    prefs.putString("pwd", s.password);
    prefs.putString("sub", s.subscribeTopic);
    prefs.putString("pub", s.publishTopic);
    prefs.putUChar("qos", s.qos);
    prefs.end();
}

void ConfigStore::clearMqtt()
{
    Preferences prefs;

    prefs.begin(kMqttNamespace, false);
    prefs.clear();
    prefs.end();
}

bool ConfigStore::hasMqttSettings()
{
    return loadMqtt().valid;
}

void ConfigStore::clearAll()
{
    clearWifi();
    clearMqtt();
}