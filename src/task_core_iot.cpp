#include "task_core_iot.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_efuse.h>

// ══════════════════════════════════════════════════════════════
//  DEVICE IDENTITY
// ══════════════════════════════════════════════════════════════
static char s_deviceName[32] = {0};

static void buildDeviceName()
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(s_deviceName, sizeof(s_deviceName),
             "ESP32-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.printf("[CoreIOT] Device name: %s\n", s_deviceName);
}

// ══════════════════════════════════════════════════════════════
//  GATEWAY DISCOVERY — mDNS + NVS cache
// ══════════════════════════════════════════════════════════════
static String s_gatewayIP = "";
static uint16_t s_gatewayPort = GATEWAY_PORT;

static bool discoverGateway()
{
    Serial.println("[CoreIOT] Scanning mDNS for gateway...");
    buildDeviceName();
    if (MDNS.begin(s_deviceName))
    {
        uint32_t start = millis();
        int found = 0;
        while (millis() - start < GATEWAY_MDNS_TIMEOUT)
        {
            found = MDNS.queryService(GATEWAY_MDNS_SERVICE, GATEWAY_MDNS_PROTO);
            if (found > 0)
                break;
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        if (found > 0)
        {
            Serial.println("[CoreIOT] Service found, resolving A Record...");

            // Ép CPU chờ phân giải IP tối đa 2 giây (20 vòng x 100ms)
            int retries = 20;
            while (MDNS.IP(0) == IPAddress(0, 0, 0, 0) && retries > 0)
            {
                vTaskDelay(pdMS_TO_TICKS(100)); // Non-blocking delay
                retries--;
            }

            if (MDNS.IP(0) != IPAddress(0, 0, 0, 0))
            {
                s_gatewayIP = MDNS.IP(0).toString();
                s_gatewayPort = MDNS.port(0);

                Serial.printf("[CoreIOT] Gateway resolved via mDNS: %s:%d\n",
                              s_gatewayIP.c_str(), s_gatewayPort);

                // Khối lệnh lưu NVS Cache của bạn để ở đây...
                Preferences prefs;
                prefs.begin("gw", false);
                prefs.putString("ip", s_gatewayIP);
                prefs.putUShort("port", s_gatewayPort);
                prefs.end();

                return true;
            }
            else
            {
                Serial.println("[CoreIOT] Timeout: Failed to resolve IPv4 address.");
            }
        }
    }

    // Fallback NVS cache
    Serial.println("[CoreIOT] mDNS failed, trying cache...");
    Preferences prefs;
    prefs.begin("gw", true);
    String cachedIP = prefs.getString("ip", "");
    uint16_t cachedPort = prefs.getUShort("port", 0);
    prefs.end();

    if (!cachedIP.isEmpty() && cachedPort > 0)
    {
        s_gatewayIP = cachedIP;
        s_gatewayPort = cachedPort;
        Serial.printf("[CoreIOT] Using cache: %s:%d\n",
                      s_gatewayIP.c_str(), s_gatewayPort);
        return true;
    }

    Serial.println("[CoreIOT] No gateway found");
    return false;
}

// ══════════════════════════════════════════════════════════════
//  PUBLISH HELPERS
// ══════════════════════════════════════════════════════════════

// Gửi connect → Gateway tạo device trên CoreIOT nếu chưa có
static void publishConnect(PubSubClient &mqtt)
{
    char topic[64];
    snprintf(topic, sizeof(topic), TOPIC_CONNECT, s_deviceName);
    mqtt.publish(topic, "{}", true); // retain=true
    Serial.printf("[CoreIOT] Connect sent → %s\n", topic);
}

// Gửi telemetry → Mosquitto → TB Gateway → CoreIOT
// Khi mất internet: TB Gateway tự buffer vào SQLite
static bool publishTelemetry(PubSubClient &mqtt, const sensor_data_t &data)
{
    char topic[64];
    snprintf(topic, sizeof(topic), TOPIC_TELEMETRY, s_deviceName);

    StaticJsonDocument<128> doc;
    doc["temperature"] = round(data.temperature * 100.0) / 100.0;

    doc["humidity"] = round(data.humidity * 100.0) / 100.0;
    doc["ml_status"] = data.ml_status;
    doc["time_stamp"] = data.timestamp_ms;
    char buf[128];
    serializeJson(doc, buf, sizeof(buf));
    return mqtt.publish(topic, buf);
}

// ══════════════════════════════════════════════════════════════
//  MAIN TASK
// ══════════════════════════════════════════════════════════════
void task_coreiot(void *pvParameter)
{
    system_se_t *sys = (system_se_t *)pvParameter;

    buildDeviceName();

    // ── Bước 1: Chờ WiFi từ task_webserver ──
    Serial.println("[CoreIOT] Waiting for WiFi...");
    xSemaphoreTake(sys->se_wifi, portMAX_DELAY);
    Serial.printf("[CoreIOT] WiFi OK — IP: %s\n",
                  WiFi.localIP().toString().c_str());

    // ── Bước 2: Tìm Gateway ──
    while (!discoverGateway())
    {
        Serial.println("[CoreIOT] Retry in 5s...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    // ── Bước 3: Setup MQTT → Mosquitto local ──
    WiFiClient wifiClient;
    PubSubClient mqtt(wifiClient);
    mqtt.setServer(s_gatewayIP.c_str(), s_gatewayPort);
    mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
    mqtt.setBufferSize(256);

    uint32_t lastPublish = 0;
    uint32_t lastReconnect = 0;

    for (;;)
    {
        // ── A. Kiểm tra WiFi ──
        wifi_status_t wst = WIFI_DISCONNECTED;
        xQueuePeek(sys->queue_wifi_status, &wst, 0);
        bool wifiOK = (wst == WIFI_CONNECTED);

        if (!wifiOK)
        {
            if (mqtt.connected())
                mqtt.disconnect();
            Serial.println("Wifi is disconnected then MQTT connected!!!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // ── B. Reconnect Mosquitto ──
        while (!mqtt.connected())
        {
            // 2. Kiểm tra chu kỳ reconnect (Tránh spam broker)
            if (millis() - lastReconnect >= RECONNECT_DELAY_MS)
            {
                lastReconnect = millis();

                Serial.printf("[CoreIOT] Connecting to Mosquitto %s:%d...\n",
                              s_gatewayIP.c_str(), s_gatewayPort);

                // 3. Thực hiện kết nối
                if (mqtt.connect("esp32"))
                {
                    Serial.println("[CoreIOT] Connected to Mosquitto!");

                    // Báo Gateway device online
                    publishConnect(mqtt);

                    // QUAN TRỌNG: Phải subscribe lại toàn bộ topic tại đây
                    // mqtt.subscribe("coreiot/downlink/command");
                }
                else
                {
                    Serial.printf("[CoreIOT] Failed rc=%d\n", mqtt.state());
                }
            }
        }

        mqtt.loop();

        // ── C. Publish telemetry mỗi PUBLISH_INTERVAL_MS ──
        if (millis() - lastPublish >= PUBLISH_INTERVAL_MS)
        {
            lastPublish = millis();

            sensor_data_t data = {0, 0, 0, 0};
            if (xQueuePeek(sys->queue_raw_data, &data, 0) == pdTRUE)
            {
                bool sent = publishTelemetry(mqtt, data);
                Serial.printf("[CoreIOT] Telemetry %s — T=%.1f H=%.1f ML=%d\n",
                              sent ? "OK" : "FAIL",
                              data.temperature, data.humidity, data.ml_status);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}