#include "global.h"
#include "LiquidCrystal_I2C.h"
#include "DHT20.h"

extern DHT20 dht20;
extern void task_read_sensor(void *pvParameter);
extern void init_task_read();
