#ifndef __NEO_BLINKY__
#define __NEO_BLINKY__
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "global.h"
#define NEO_PIN 45
#define LED_COUNT 1

void neo_blinky(void *pvParameters);
extern void task_humi_neo(void *pvParameter);
extern void task_humi_neo_state(void *pvParameter);
#endif