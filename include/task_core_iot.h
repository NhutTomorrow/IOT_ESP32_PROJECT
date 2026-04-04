#ifndef __TASK_CORE_IOT_H__
#define __TASK_CORE_IOT_H__

#include "global.h"
#include <Arduino.h>

// ══════════════════════════════════════════════════════════════
//  LUỒNG DỮ LIỆU
//
//  ESP32 ──MQTT──► Mosquitto:1883 (local PC)
//                       │
//                  TB Gateway (Docker)
//                       │
//              ┌─────────────────────┐
//              │ Có internet         │──► app.coreiot.io
//              │ Mất internet        │──► SQLite (tb-gateway/data/)
//              │ Có internet trở lại │──► drain SQLite → CoreIOT
//              └─────────────────────┘
//
//  ESP32 chỉ gửi lên Mosquitto local.
//  TB Gateway lo toàn bộ buffer + forward.
//  Device name lấy từ TOPIC (regex trong mqtt.json):
//    "devices/(?P<deviceName>[^/]+)/telemetry"
// ══════════════════════════════════════════════════════════════

#define GATEWAY_PORT 1883
#define GATEWAY_MDNS_SERVICE "_mqtt"
#define GATEWAY_MDNS_PROTO "_tcp"
#define GATEWAY_MDNS_TIMEOUT 8000

#define TOPIC_CONNECT "devices/%s/connect"
#define TOPIC_TELEMETRY "devices/%s/telemetry"

#define PUBLISH_INTERVAL_MS 5000
#define RECONNECT_DELAY_MS 5000
#define MQTT_KEEPALIVE_S 60

void task_coreiot(void *pvParameter);

#endif