#pragma once
#include <Arduino.h>

// WiFi station credentials, as collected by the WiFi captive portal.
struct WifiCredentials {
  String ssid;
  String password;
  bool   valid = false; // true once loaded from NVS and non-empty
};

// MQTT broker connection + topic settings, as collected by the MQTT broker portal.
struct MqttSettings {
  String  host;
  uint16_t port        = 1883;
  String  clientId;       // left empty -> auto-generated from chip MAC
  String  username;       // optional
  String  password;       // optional
  String  subscribeTopic; // topic the device listens on (commands)
  String  publishTopic;   // topic the device publishes to (telemetry / status)
  uint8_t qos          = 2; // 0, 1, or 2
  bool    valid         = false; // true once loaded from NVS and host is set
};
