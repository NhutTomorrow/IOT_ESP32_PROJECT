#include "task_read.h"

void init_task_read()
{

    Wire.begin(SDA_I2C, SCL_I2C);
    Serial.begin(115200);
    dht20.begin();
    Serial.println("init success!!!!");
}

void task_read_sensor(void *pvParameter)
{
    init_task_read();
    // declare system semaphore and sensor data
    sensor_data_t current_data;
    system_se_t *sys_se = (system_se_t *)pvParameter;

    while (1)
    {
        dht20.read();
        float temp = dht20.getTemperature();
        float humi = dht20.getHumidity();

        // if (isnan(temp) || isnan(humi))
        // {
        //     Serial.println("Failed to read from DHT sensor!");
        //     vTaskDelay(pdMS_TO_TICKS(2000));
        //     continue;
        // }
        BaseType_t status = xQueueSend(sys_se->queue_raw_data, &current_data, pdMS_TO_TICKS(100));

        if (status == pdPASS)
        {
            Serial.println("Đã gửi dữ liệu vào Queue thành công!");
        }
        else
        {
            Serial.println("Queue đầy, gửi thất bại!");
        }
        current_data.temperature = temp;
        current_data.humidity = humi;
        if (current_data.temperature < 30.0)
        {
            xSemaphoreGive(sys_se->se_temp_normal);
        }
        else if (current_data.temperature >= 30 && current_data.humidity < 35)
        {
            xSemaphoreGive(sys_se->se_temp_warning);
        }
        else
        {
            xSemaphoreGive(sys_se->se_temp_critical);
        }

        if (current_data.humidity < 40.0)
        {
            xSemaphoreGive(sys_se->se_humi_critical);
        }
        else if (current_data.humidity >= 40 && current_data.humidity <= 70)
        {
            xSemaphoreGive(sys_se->se_humi_normal);
        }
        else
        {
            xSemaphoreGive(sys_se->se_humi_warning);
        }

        // write done -> return token critical section
        Serial.print("Humidity: ");
        Serial.print(current_data.humidity);
        Serial.print("%  Temperature: ");
        Serial.print(current_data.temperature);
        Serial.println("°C");

        vTaskDelay(pdMS_TO_TICKS(2000)); // config from web sever to select automatic read, read  only one time???
    }
}