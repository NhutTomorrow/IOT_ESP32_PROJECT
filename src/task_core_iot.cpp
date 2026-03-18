
#include "task_core_iot.h"
// ════════════════════════════════
//  Build JSON payload
// ════════════════════════════════
static void buildPayload(sensor_data_t *data,
                         char *buf, size_t len)
{
    const char *mlStr = (data->ml_status == 0) ? "Normal" : (data->ml_status == 1) ? "Noisy"
                                                                                   : "Unreliable";

    snprintf(buf, len,
             "{"
             "\"temperature\":%.1f,"
             "\"humidity\":%.1f,"
             "\"ml_status\":\"%s\""
             "}",
             data->temperature,
             data->humidity,
             mlStr);
}

// ════════════════════════════════
//  RTOS Task
// ════════════════════════════════
void task_cloud(void *pvParameter)
{
    system_se_t *sys_se = (system_se_t *)pvParameter;

    WiFiClient wifiClient;
    PubSubClient mqtt(wifiClient);
    mqtt.setServer(MOSQUITTO_HOST, MOSQUITTO_PORT);
    mqtt.setKeepAlive(60);
    mqtt.setSocketTimeout(10);

    // ── Hàm kết nối Mosquitto ──
    // Anonymous — không cần token
    // Token được Node-RED thêm vào khi đẩy lên CoreIOT
    auto connectMQTT = [&]() -> bool
    {
        int retry = 0;
        while (!mqtt.connected() && retry < 5)
        {
            Serial.printf("[Cloud] Connecting Mosquitto"
                          " (try %d/5)...\n",
                          retry + 1);

            if (mqtt.connect("ESP32_Group01"))
            {
                Serial.println("[Cloud] Mosquitto connected!");
                Serial.println("[Cloud] Topic: " TOPIC_DATA);
                return true;
            }

            Serial.printf("[Cloud] Failed rc=%d → retry\n",
                          mqtt.state());
            retry++;
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        Serial.println("[Cloud] Cannot connect Mosquitto!");
        return false;
    };

    // ── Chờ WiFi từ taskWeb ──
    Serial.println("[Cloud] Waiting for WiFi...");
    xSemaphoreTake(sys_se->se_wifi, portMAX_DELAY);
    Serial.println("[Cloud] WiFi ready → connect Mosquitto");

    connectMQTT();

    // ── Nhận token mới từ Web (nếu cần dùng sau) ──
    // cloud_config_t newCfg;

    sensor_data_t data;
    char payload[256];
    uint32_t lastPublish = 0;

    while (1)
    {
        // ── 1. Nhận token mới từ Web settings ──
        // if (xQueueReceive(sys_se->queue_new_token,
        //                   &newCfg, 0) == pdTRUE)
        // {
        //     // Token được Node-RED dùng để đẩy CoreIOT
        //     // ESP32 không cần dùng trực tiếp
        //     // Nhưng lưu lại để log
        //     Serial.println("[Cloud] Token updated in Node-RED");
        // }

        // ── 2. Kiểm tra WiFi ──
        wifi_status_t wst = WIFI_DISCONNECTED;
        xQueuePeek(sys_se->queue_wifi_status, &wst, 0);

        if (wst != WIFI_CONNECTED)
        {
            Serial.println("[Cloud] WiFi lost → waiting...");
            xSemaphoreTake(sys_se->se_wifi,
                           pdMS_TO_TICKS(1000));
            continue;
        }

        // ── 3. Reconnect Mosquitto nếu mất ──
        if (!mqtt.connected())
        {
            Serial.println("[Cloud] Mosquitto lost → reconnect");
            if (!connectMQTT())
            {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }

        // ── 4. Giữ kết nối MQTT sống ──
        mqtt.loop();

        // ── 5. Publish mỗi PUBLISH_MS ──
        if (millis() - lastPublish >= PUBLISH_MS)
        {
            lastPublish = millis();

            // Đọc sensor từ queue
            // tạm thời ch
            if (xQueuePeek(sys_se->queue_raw_data,
                           &data, 0) != pdTRUE)
            {
                Serial.println("[Cloud] No sensor data yet");
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            // Build payload
            buildPayload(&data, payload, sizeof(payload));

            // Publish lên Mosquitto → Node-RED nhận
            if (mqtt.publish(TOPIC_DATA, payload, false))
            {
                Serial.printf("[Cloud] Published → %s : %s\n",
                              TOPIC_DATA, payload);
            }
            else
            {
                Serial.println("[Cloud] Publish failed!"
                               " Check Mosquitto.");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}