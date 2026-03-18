#pragma once
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include "global.h"

// ── Mosquitto Docker trên máy Windows ──
#define MOSQUITTO_HOST "192.168.0.15" // ← IP máy tính window  của bạn
#define MOSQUITTO_PORT 1883
#define TOPIC_DATA "esp32/sensors" // ← khớp Node-RED

// ── Publish interval ──
#define PUBLISH_MS 5000
extern void task_cloud(void *pvParameter);