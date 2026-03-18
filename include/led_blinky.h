#ifndef __LED_BLINKY__
#define __LED_BLINKY__
#include <Arduino.h>
#include "global.h"
#define LED_GPIO 48
void led_blinky(void *pvParameters);

extern void task_temp_blink(void *pvParameter);

#endif