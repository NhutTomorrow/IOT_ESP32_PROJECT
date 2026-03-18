#include "task_read.h"
DHT20 dht20;
void init_task_read()
{

    Wire.begin(SDA_I2C, SCL_I2C);
    // Scan I2C
    // Serial.println("[I2C] Scanning...");
    // for (byte addr = 1; addr < 127; addr++)
    // {
    //     Wire.beginTransmission(addr);
    //     if (Wire.endTransmission() == 0)
    //         Serial.printf("[I2C] Found: 0x%02X\n", addr);
    // }

    dht20.begin();
    vTaskDelay(pdMS_TO_TICKS(2000)); // ← chờ 2s
    Serial.println("init success!!!!");
}

void task_read_sensor(void *pvParameter)
{
    // init_task_read();
    // declare system semaphore and sensor data
    sensor_data_t current_data;
    system_se_t *sys_se = (system_se_t *)pvParameter;
    // Kiểm tra ngay đầu task
    Serial.printf("[DEBUG] se_i2c         : %p\n", sys_se->se_i2c);
    Serial.printf("[DEBUG] se_temp_normal : %p\n", sys_se->se_temp_normal);
    Serial.printf("[DEBUG] se_humi_normal : %p\n", sys_se->se_humi_normal);
    Serial.printf("[DEBUG] queue_raw_data : %p\n", sys_se->queue_raw_data);
    while (1)
    {
        // Lấy mutex I2C
        if (xSemaphoreTake(sys_se->se_i2c,
                           pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            int status = dht20.read();

            if (status == 0)
            {
                current_data.temperature = dht20.getTemperature();
                current_data.humidity = dht20.getHumidity();
                current_data.ml_status = 0;

                xQueueOverwrite(sys_se->queue_raw_data,
                                &current_data);

                // Semaphore nhiệt độ
                if (current_data.temperature < 30.0)
                    xSemaphoreGive(sys_se->se_temp_normal);
                else if (current_data.temperature < 35.0)
                    xSemaphoreGive(sys_se->se_temp_warning);
                else
                    xSemaphoreGive(sys_se->se_temp_critical);

                // Semaphore độ ẩm
                if (current_data.humidity < 40.0)
                    xSemaphoreGive(sys_se->se_humi_critical);
                else if (current_data.humidity <= 70.0)
                    xSemaphoreGive(sys_se->se_humi_normal);
                else
                    xSemaphoreGive(sys_se->se_humi_warning);
                Serial.printf("[Sensor] T:%.1f H:%.1f\n",
                              current_data.temperature,
                              current_data.humidity);
            }
            else
            {
                Serial.printf("[Sensor] Read failed: %d\n",
                              status);
            }

            xSemaphoreGive(sys_se->se_i2c);
        }
        else
        {
            Serial.println("[Sensor] I2C mutex timeout");
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // ← bắt buộc
    }
}