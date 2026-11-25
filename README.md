# ESP32 WiFi Portal + MQTT Broker Portal (QoS 0/1/2)

A modular PlatformIO/Arduino project for ESP32 with two onboarding web portals:

1. **WiFi portal** - A SoftAP and captive portal starts when no WiFi credentials
   are stored or when the saved network cannot be reached. Connect to the
   ESP32 network and enter the home WiFi SSID and password.
2. **MQTT broker portal** - A normal web page is served on the ESP32's LAN IP
   after WiFi connects. It collects the broker host, port, optional credentials,
   topics, and QoS level.

Both portals write settings to flash (NVS via `Preferences`) and reboot the
device after submission. The ESP32 then connects to WiFi and MQTT and publishes
or subscribes at the configured QoS.

## MQTT QoS support

This project uses `cyijun/ESP32MQTTClient`, a wrapper around Espressif's native
ESP-IDF `esp-mqtt` component. It supports genuine QoS 0, 1, and 2:

| QoS | Delivery behavior |
| --- | --- |
| 0 | At most once |
| 1 | At least once |
| 2 | Exactly once, with PUBREC/PUBREL/PUBCOMP handshaking |

The PubSubClient library is not used because it does not implement the QoS 2
handshake.

## Project layout

```text
platformio.ini
include/
  Config.h             - WifiCredentials and MqttSettings structures
src/
  ConfigStore.h/.cpp   - NVS storage for WiFi and MQTT settings
  WiFiPortal.h/.cpp    - SoftAP and captive WiFi setup portal
  MqttPortal.h/.cpp    - LAN MQTT broker configuration portal
  NetworkCatalog.h/.cpp- allowed WiFi networks and default fallback network
  MqttManager.h/.cpp   - MQTT connection, QoS publish/subscribe, and callbacks
  main.cpp             - boot state machine
```

## Boot flow

```text
power on
  -> hold BOOT (GPIO0) for 3 seconds -> wipe NVS and restart
  -> WiFi credentials stored?
       no  -> start WiFi portal (AP "ESP32-Setup", http://192.168.4.1)
              submit form -> save credentials -> restart
       yes -> connect to WiFi
              if connection fails -> start WiFi portal
  -> MQTT broker settings stored?
       no  -> start MQTT portal on the ESP32 LAN IP
              submit form -> save settings -> restart
       yes -> MqttManager.begin() -> connect, subscribe, and publish
```

## Default MQTT settings

| Setting | Default |
| --- | --- |
| Broker | `test.mosquitto.org` |
| Port | `1883` |
| Client ID | `esp32-device-01` |
| Subscribe topic | `esp32/device-01/#` |
| Publish topic | `esp32/device-01/status` |
| QoS | `2` |

WiFi remote configuration topic:

```text
esp32/device-01/config/wifi
```

MQTT remote restart topic:

```text
esp32/device-01/config/mqtt
```

WiFi configuration payload:

```json
{
  "ssid": "MyWiFi",
  "password": "MyPassword"
}
```

Remote reboot payload:

```json
{
  "command": "REBOOT"
}
```

## Status payload

The ESP32 publishes every five seconds to `esp32/device-01/status`:

```json
{
  "uptime": 12345,
  "rssi": -45
}
```

- `uptime` is the ESP32 uptime in milliseconds.
- `rssi` is the current WiFi received signal strength.

## Build and flash

```powershell
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Adjust `board` in `platformio.ini` for your ESP32 board, such as
`esp32-s3-devkitc-1` or `esp32-c3-devkitm-1`.

## First-time setup

1. Flash the firmware and power on the ESP32.
2. Connect a phone or laptop to WiFi network `ESP32-Setup`.
3. Open the captive portal, or browse to `http://192.168.4.1`.
4. Enter the home WiFi SSID and password and submit.
5. The ESP32 reboots and connects to the WiFi network.
6. Watch the Serial monitor for the ESP32's LAN IP address.
7. Browse to `http://<ESP32-LAN-IP>` from a device on the same network.
8. Enter the MQTT broker host, port, optional credentials, subscribe/publish
   topics, and QoS (0, 1, or 2).
9. Submit to save the MQTT settings. The ESP32 reboots and connects to the broker.

To reconfigure later, hold the BOOT button for approximately three seconds
during startup to erase all WiFi and MQTT settings.
