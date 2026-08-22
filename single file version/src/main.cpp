//  Set to 0 silences ALL Serial output in the entire firmware and 1 to enables all Serial printing as normal
#define DEBUG 1

#define LOG(fmt, ...)  do { if (DEBUG) Serial.printf(fmt, ##__VA_ARGS__); } while (0)
#define LOGLN(x)       do { if (DEBUG) Serial.println(x);                 } while (0)


//NETWORK CATALOG
#define DEFAULT_SSID        "Himaa"
#define DEFAULT_PASS        "Hima_2002"

static const char* const kAllowedSSIDs[] = {
    "Hima", "Himaa", "Dialog 4G 101",
    "Office_WiFi", "Factory_WiFi", "Lab_WiFi"
};
#define ALLOWED_SSID_COUNT  (sizeof(kAllowedSSIDs) / sizeof(kAllowedSSIDs[0]))

//MQTT
#define MQTT_HOST        "test.mosquitto.org"
#define MQTT_PORT        1883
#define MQTT_CLIENT_ID   "esp32-himara-01"
#define MQTT_SUB_TOPIC   "himara/esp32-01/#"
#define MQTT_PUB_TOPIC   "himara/esp32-01/status"
#define MQTT_QOS         2
#define MQTT_KEEPALIVE   30

#define NVS_WIFI_NS  "wifi_cfg"
#define NVS_MQTT_NS  "mqtt_cfg"

#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_RETRY_INTERVAL_MS      500
#define WIFI_MONITOR_INTERVAL_MS   5000
#define PUBLISH_INTERVAL_MS        5000

//  Task               Stack   Priority   Core
//  ─────────────────  ──────  ────────   ────
//  WiFi Monitor       3072       3         0   ← manages connection / reconnect
//  MQTT Publish       3072       3         0   ← periodic status publisher
//  MQTT Client*       4096       5         1   ← internal to ESP32MQTTClient::loopStart()

#define WIFI_TASK_STACK   3072
#define WIFI_TASK_PRIO       3
#define WIFI_TASK_CORE       0

#define PUB_TASK_STACK    3072
#define PUB_TASK_PRIO        3
#define PUB_TASK_CORE        0


#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESP32MQTTClient.h>
#include <ArduinoJson.h>


static ESP32MQTTClient mqttClient;
static String          gPubTopic;
static String          gSubTopic;
static uint8_t         gQos = MQTT_QOS;


struct WifiCreds { String ssid, pass; bool valid; };
struct MqttCfg   { String host, clientId, user, pass, sub, pub;
                   uint16_t port; uint8_t qos; bool valid; };

static WifiCreds loadWifi() {
    WifiCreds c; Preferences p;
    p.begin(NVS_WIFI_NS, true);
    c.ssid = p.getString("ssid", ""); c.pass = p.getString("pass", "");
    p.end();
    c.valid = c.ssid.length() > 0;
    return c;
}

static void saveWifi(const String& ssid, const String& pass) {
    Preferences p; p.begin(NVS_WIFI_NS, false);
    p.putString("ssid", ssid); p.putString("pass", pass);
    p.end();
}

static MqttCfg loadMqtt() {
    MqttCfg s; Preferences p;
    p.begin(NVS_MQTT_NS, true);
    s.host = p.getString("host", "");   s.port      = p.getUShort("port", MQTT_PORT);
    s.clientId  = p.getString("cid",  "");   s.user      = p.getString("user", "");
    s.pass      = p.getString("pwd",  "");   s.sub       = p.getString("sub",  "");
    s.pub       = p.getString("pub",  "");   s.qos       = p.getUChar ("qos",  MQTT_QOS);
    p.end();
    s.valid = s.host.length() > 0;
    return s;
}

static void saveMqtt(const MqttCfg& s) {
    Preferences p; p.begin(NVS_MQTT_NS, false);
    p.putString("host", s.host);    p.putUShort("port", s.port);
    p.putString("cid",  s.clientId); p.putString("user", s.user);
    p.putString("pwd",  s.pass);    p.putString("sub",  s.sub);
    p.putString("pub",  s.pub);     p.putUChar ("qos",  s.qos);
    p.end();
}

static void clearAllNVS() {
    Preferences p;
    p.begin(NVS_WIFI_NS, false); p.clear(); p.end();
    p.begin(NVS_MQTT_NS, false); p.clear(); p.end();
}

//NETWORK CATALOG
static bool isAllowedSSID(const String& ssid) {
    for (size_t i = 0; i < ALLOWED_SSID_COUNT; i++)
        if (ssid == kAllowedSSIDs[i]) return true;
    return false;
}

//MQTT INCOMING MESSAGE HANDLER - Handles /config/wifi and /config/mqtt command topics
static void onMessage(const std::string& topic, const std::string& payload) {
    String t(topic.c_str()), p(payload.c_str());
    LOG("[MQTT IN] %s  →  %s\n", t.c_str(), p.c_str());

    JsonDocument doc;
    if (deserializeJson(doc, p)) { LOGLN("[MQTT] JSON parse error"); return; }

    if (t.endsWith("/config/wifi")) {
        String ssid = doc["ssid"].as<String>();
        String pass = doc["password"].as<String>();
        if (isAllowedSSID(ssid)) {
            saveWifi(ssid, pass);
            LOGLN("[MQTT] New WiFi creds saved — restarting");
            delay(1000); ESP.restart();
        } else {
            LOGLN("[MQTT] SSID rejected by catalog");
        }
    }
    else if (t.endsWith("/config/mqtt")) {
        if (String(doc["command"].as<const char*>()) == "REBOOT") {
            LOGLN("[MQTT] Remote reboot received");
            delay(1000); ESP.restart();
        }
    }
}

//MQTT BROKER CONNECTION CALLBACKS - Required by ESP32MQTTClient — must be global (not static)
void onMqttConnect(esp_mqtt_client_handle_t client) {
    if (!mqttClient.isMyTurn(client)) return;
    LOGLN("[MQTT] Connected to broker");
    if (gSubTopic.length() > 0)
        mqttClient.subscribe(std::string(gSubTopic.c_str()), onMessage, gQos);
}

esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
    mqttClient.onEventCallback(event);
    return ESP_OK;
}

//WiFi Monitor  [Core 0] - Connects on boot; re-connects automatically if the link drops
static void taskWifiMonitor(void*) {
    WifiCreds creds = loadWifi();
    if (!creds.valid) {
        LOGLN("[WiFi] No stored creds — using default");
        creds.ssid = DEFAULT_SSID; creds.pass = DEFAULT_PASS;
        saveWifi(creds.ssid, creds.pass);
    }

    WiFi.mode(WIFI_STA);

    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            LOG("[WiFi] Connecting to \"%s\"...\n", creds.ssid.c_str());
            WiFi.begin(creds.ssid.c_str(), creds.pass.c_str());

            uint32_t start = millis();
            while (WiFi.status() != WL_CONNECTED &&
                   millis() - start < WIFI_CONNECT_TIMEOUT_MS)
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS));

            if (WiFi.status() == WL_CONNECTED)
                LOG("[WiFi] Connected  IP: %s\n", WiFi.localIP().toString().c_str());
            else
                LOGLN("[WiFi] Timeout — will retry");
        }
        vTaskDelay(pdMS_TO_TICKS(WIFI_MONITOR_INTERVAL_MS));
    }
}
// MQTT Publish  [Core 0] - Publishes uptime + RSSI to the status topic every PUBLISH_INTERVAL_MS

static void taskMqttPublish(void*) {
    for (;;) {
        if (mqttClient.isConnected()) {
            char buf[96];
            snprintf(buf, sizeof(buf),
                     "{\"uptime\":%lu,\"rssi\":%d}",
                     millis(), WiFi.RSSI());
            mqttClient.publish(std::string(gPubTopic.c_str()), std::string(buf), gQos, false);
            LOG("[MQTT PUB] %s\n", buf);
        }
        vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
    }
}
//  SETUP

void setup() {
    if (DEBUG) { Serial.begin(115200); delay(500); }

    // clearAllNVS(); // uncomment to wipe NVS for a clean test run

    //Spawn WiFi Monitor (Core 0)
    xTaskCreatePinnedToCore(taskWifiMonitor, "WiFi", WIFI_TASK_STACK,
                            nullptr, WIFI_TASK_PRIO, nullptr, WIFI_TASK_CORE);

    // Block here until WiFi is up before initialising MQTT
    LOGLN("[Setup] Waiting for WiFi...");
    while (WiFi.status() != WL_CONNECTED)
        vTaskDelay(pdMS_TO_TICKS(200));

    //Seed MQTT defaults into NVS if not already configured 
    if (!loadMqtt().valid) {
        MqttCfg d = { MQTT_HOST, MQTT_CLIENT_ID, "", "", MQTT_SUB_TOPIC, MQTT_PUB_TOPIC,
                       MQTT_PORT, MQTT_QOS, true };
        saveMqtt(d);
        LOGLN("[MQTT] Default config written to NVS");
    }

    MqttCfg cfg = loadMqtt();
    gPubTopic = cfg.pub; gSubTopic = cfg.sub; gQos = cfg.qos;

    if (cfg.clientId.length() == 0) {   // auto-generate ID from MAC if blank
        char id[32]; snprintf(id, sizeof(id), "esp32-%012llX", ESP.getEfuseMac());
        cfg.clientId = id;
    }

    //Configure and start MQTT client (internal task → Core 1) 
    mqttClient.setURL(cfg.host.c_str(), cfg.port, cfg.user.c_str(), cfg.pass.c_str());
    mqttClient.setMqttClientName(cfg.clientId.c_str());
    mqttClient.setKeepAlive(MQTT_KEEPALIVE);
    mqttClient.setAutoReconnect(true);
    mqttClient.setOnMessageCallback(onMessage);
    LOGLN("[MQTT] Starting client (Core 1 internal task)...");
    mqttClient.loopStart();

    //Spawn MQTT Publish task (Core 0) 
    xTaskCreatePinnedToCore(taskMqttPublish, "MqttPub", PUB_TASK_STACK,
                            nullptr, PUB_TASK_PRIO, nullptr, PUB_TASK_CORE);
}
//  LOOP - All work is handled by FreeRTOS tasks

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
