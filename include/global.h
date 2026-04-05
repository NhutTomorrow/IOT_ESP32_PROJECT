#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// extern float glob_temperature;
// extern float glob_humidity;

// extern String WIFI_SSID;
// extern String WIFI_PASS;
// extern String CORE_IOT_TOKEN;
// extern String CORE_IOT_SERVER;
// extern String CORE_IOT_PORT;

// extern boolean isWifiConnected;
// extern SemaphoreHandle_t xBinarySemaphoreInternet;
#define WINDOW_SIZE 5
#define COLLECT_MS 2000 // đọc mỗi 500ms
#define SDA_I2C 11
#define SCL_I2C 12
typedef struct
{
    float temperature;
    float humidity;
    int ml_status;
    uint32_t timestamp_ms;
} sensor_data_t;

// ── Struct lệnh điều khiển từ Web ──
typedef struct
{
    bool state; // true = ON / false = OFF
} led_cmd_t;

typedef struct
{
    uint8_t r, g, b; // màu NeoPixel
} neo_cmd_t;

// ── Struct config WiFi + Token ──
typedef struct
{
    char ssid[64];
    char pass[64];
    char token[128]; // CoreIOT Device Token
} wifi_info_t;
// ── Trạng thái WiFi ──
typedef enum
{
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED
} wifi_status_t;
// ── Kết quả TinyML ──
typedef struct
{
    int label;        // 0=normal, 1=warning, 2=critical
    float confidence; // độ chính xác 0.0 ~ 1.0
} ml_result_t;
typedef enum
{
    INIT_LED_TEMP,
    STATE_NORMAL,
    STATE_WARNING,
    STATE_CRITICAL,
    LED_MODE_AUTO,  // Task 1 điều khiển theo nhiệt độ
    LED_MODE_MANUAL // Web điều khiển, Task 1 bị block
} led_mode_t;

typedef enum
{
    INIT_HUMI_NEO,
    STATE_NORMAL_NEO,
    STATE_WARNING_NEO,
    STATE_CRITICAL_NEO,
    NEO_MODE_AUTO,  // Task 2 điều khiển theo độ ẩm
    NEO_MODE_MANUAL // Web điều khiển, Task 2 bị block
} neo_mode_t;

typedef enum
{
    INIT_LCD_MONITOR,
    NORMAL_MONITOR,
    WARNING_MONITOR,
    CRITICAL_MONITOR,
    AUTO_MONITOR,
    MANUAL_MONITOR
} lcd_mode_t;
typedef enum
{
    WEB_WIFI_INIT,
    WEB_WIFI_CONNECTING,
    WEB_WIFI_RECONNECTING,
    WEB_WIFI_AP,
    WEB_WIFI_CONNECTED
} web_wifi_t;
typedef struct
{
    // ══════════════════════════════
    //  SEMAPHORE TASK 1 — LED + Nhiệt độ
    // ══════════════════════════════
    SemaphoreHandle_t se_temp_normal;
    SemaphoreHandle_t se_temp_warning;
    SemaphoreHandle_t se_temp_critical;

    // ══════════════════════════════
    //  SEMAPHORE TASK 2 — NeoPixel + Độ ẩm
    // ══════════════════════════════
    SemaphoreHandle_t se_humi_normal;
    SemaphoreHandle_t se_humi_warning;
    SemaphoreHandle_t se_humi_critical;

    // ══════════════════════════════
    //  SEMAPHORE TASK 3 — LCD Display
    // ══════════════════════════════
    SemaphoreHandle_t se_i2c;

    // ══════════════════════════════
    //  SEMAPHORE TASK 4 — Web Server
    // ══════════════════════════════
    SemaphoreHandle_t se_wifi; // Binary: báo hiệu WiFi đã kết nối xong
                               // taskWifi → Give / taskCloud → Take

    // ══════════════════════════════
    //  SEMAPHORE TASK 5 — TinyML
    // ══════════════════════════════
    SemaphoreHandle_t se_ml_ready; // Binary: báo inference xong
    // taskML → Give / taskLCD, taskCloud → Take

    // ══════════════════════════════
    //  QUEUE — vận chuyển dữ liệu
    // ══════════════════════════════
    // Thêm vào struct
    QueueHandle_t queue_led_mode; // Web → taskLED
    QueueHandle_t queue_neo_mode; // Web → taskNeo
    // Task 1, 2, 3 (đã có)
    QueueHandle_t queue_raw_data; // Sensor → Task1, Task2, Task3, Task5

    // Task 4 — lệnh từ Web
    QueueHandle_t queue_led_cmd;     // Web → taskLED  (led_cmd_t)
    QueueHandle_t queue_neo_cmd;     // Web → taskNeo  (neo_cmd_t)
    QueueHandle_t queue_wifi_config; // Web → taskWifi (wifi_config_t)
    QueueHandle_t queue_wifi_status; // taskWifi → Web, Cloud (wifi_status_t)

    // Task 5 — kết quả ML

    // Task 6 — publish lên CoreIOT (đã có)
    QueueHandle_t queue_publish_data; // taskSensor+ML → taskCloud

} system_se_t;

/*
taskSensor ──[queue_raw_data]──────→ taskLED   (Task 1)
               │                 └──→ taskNeo   (Task 2)
               │                 └──→ taskLCD   (Task 3)
               │                 └──→ taskML    (Task 5)
               │
               └──[queue_publish_data]──→ taskCloud (Task 6)

taskWeb ──[queue_led_cmd]──→ taskLED       se_LED1 bảo vệ hardware
        ├─[queue_neo_cmd]──→ taskNeo       se_LED2 bảo vệ hardware
        └─[queue_wifi_config]→ taskWifi

taskWifi──[queue_wifi_status]→ taskWeb, taskCloud
        └──[se_wifi Give]──────→ taskCloud (báo bắt đầu MQTT)

taskML ──[queue_ml_result]──→ taskLCD      se_ml_ready đồng bộ
       └──────────────────────→ taskCloud
*/
#endif