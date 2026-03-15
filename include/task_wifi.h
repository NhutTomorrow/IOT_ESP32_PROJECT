#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include <WiFi.h>
#include <task_check_info.h>

#define AP_SSID "SMART ESP32 CONFIG"
#define AP_PASSWORD "03057071"

extern bool Wifi_reconnect();
extern void startAP();

#endif