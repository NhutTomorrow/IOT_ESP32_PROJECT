#include "global.h"
#include "LiquidCrystal_I2C.h"
#include "DHT20.h"

#define SDA_I2C 11
#define SCL_I2C 12
DHT20 dht20;
void task_read_sensor(void *pvParameter);
