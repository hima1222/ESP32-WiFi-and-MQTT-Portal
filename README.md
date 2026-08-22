# ESP32 WiFi Portal + MQTT Broker Portal (QoS 2)

A modular PlatformIO/Arduino project for ESP32 with two onboarding web portals:

1. **WiFi portal** — SoftAP + captive portal, shown when no WiFi credentials are
   stored. You connect a phone/laptop to the ESP32's own AP and type in your
   home WiFi SSID/password.
2. **MQTT broker portal** — a normal web page served on the device's LAN IP
   (once WiFi is up), used to enter the MQTT broker host, port, credentials,
   topics, and QoS level (0/1/2).

Both portals just write to flash (NVS via `Preferences`) and reboot. After both
are configured, the device connects to the broker and publishes/subscribes at
**genuine QoS 2**.

## Why this MQTT library, not PubSubClient

`PubSubClient` — the library almost every ESP32 MQTT tutorial uses — can only
**publish at QoS 0**, and can only **subscribe at QoS 0 or 1**. It has no real
QoS 2 implementation (no PUBREC/PUBREL/PUBCOMP handshake), regardless of what
number you pass it.

Real QoS 2 on ESP32 exists in Espressif's own `esp-mqtt` component (the native
ESP-IDF MQTT client), which implements all three QoS levels properly. This
project uses [`cyijun/ESP32MQTTClient`](https://github.com/cyijun/ESP32MQTTClient),
a small Arduino-friendly wrapper around that exact component — so you get true
QoS 2 while staying inside a normal `framework = arduino` PlatformIO project
(no need for the more fragile combined `arduino + espidf` build).

## Project layout

```
platformio.ini
include/
  Config.h          - WifiCredentials / MqttSettings structs
src/
  ConfigStore.h/.cpp - NVS (Preferences) read/write for both settings
  WiFiPortal.h/.cpp   - SoftAP + captive portal for WiFi setup
  MqttPortal.h/.cpp   - LAN web form for broker host/port/topics/QoS
  MqttManager.h/.cpp  - wraps ESP32MQTTClient, real QoS 0/1/2 pub/sub
  main.cpp            - boot state machine tying it all together
```

## Boot flow

```
power on
  -> hold BOOT (GPIO0) 3s? -> wipe NVS, restart
  -> WiFi creds saved & connect OK?
       no  -> start WiFi portal (AP "ESP32-Setup", http://192.168.4.1)
              user submits form -> save -> restart
       yes -> continue
  -> MQTT broker settings saved?
       no  -> start MQTT portal on the device's LAN IP (see Serial monitor)
              user submits form -> save -> restart
       yes -> MqttManager.begin() -> connect, subscribe, publish at QoS
```

## Build & flash

```bash
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Adjust `board` in `platformio.ini` if you're not using a generic ESP32 dev
board (e.g. `esp32-s3-devkitc-1`, `esp32-c3-devkitm-1`).

## First-time setup on the device

1. Flash and power up. Connect your phone to WiFi network **ESP32-Setup**
   (open the captive portal page, or browse to `192.168.4.1`).
2. Enter your home WiFi SSID/password, submit. The device reboots and joins
   that network.
3. Watch the Serial monitor for the device's new IP address, e.g.
   `192.168.1.42`. Browse to `http://192.168.1.42` from a device on the same
   network.
4. Enter your MQTT broker host/IP, port, optional credentials, the
   subscribe/publish topics, and QoS (defaults to 2). Submit. The device
   reboots and connects to the broker.
5. From then on it auto-reconnects to both WiFi and MQTT on every boot.

To reconfigure later: hold the BOOT button for ~3 seconds at power-on to wipe
both WiFi and MQTT settings and start over from step 1.

## Customizing

- `MqttManager::handleIncomingMessage()` in `MqttManager.cpp` is where you'd
  parse incoming command payloads (currently just logs them).
- `main.cpp`'s `loop()` builds a tiny JSON status payload every 5 seconds and
  publishes it via `MqttManager::publishStatus()` at the configured QoS — swap
  that for your actual sensor data.
- TLS (port 8883) is supported by the underlying library
  (`ESP32MQTTClient::setCaCert()`) if you need an encrypted broker connection;
  it's omitted here to keep the example focused.
