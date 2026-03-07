#ifndef __TEMP_HUMI_MONITOR__
#define __TEMP_HUMI_MONITOR__
#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "DHT20.h"
#include "global.h"

#define SDA_GPIO 13
#define SCL_GPIO 14 // de tam thoi
void temp_humi_monitor(void *pvParameters);
void task_temp_humi_monitor(void *pvParameter);
void normal_monitor(float temp, float humi);
void alert_monitor(float temp, float humi);
void warning_monitor(float temp, float humi);
#endif