#include "task_read.h"
void task_read_sensor(void *pvParameter)
{
    Sensor_data *share_data = (Sensor_data *)pvParameter;
    Wire.begin(SDA_I2C, SCL_I2C);
    Serial.begin(115200);
    dht20.begin();
    while (1)
    {
        dht20.read();
        if (isnan(dht20.getHumidity()) || isnan(dht20.getTemperature()))
        {
            Serial.println("Failed to read from DHT sensor!");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        if (xSemaphoreTake(share_data->se_data, (TickType_t)100) == pdTRUE)
        {
            share_data->humidity = dht20.getHumidity();
            share_data->temperature = dht20.getTemperature();
            if (share_data->humidity < 40.0 || share_data->temperature > 35)
            {
                xSemaphoreGive(share_data->se_alert); // alert for other task
            }
            // write done -> return token critical section
            Serial.print("Humidity: ");
            Serial.print(share_data->humidity);
            Serial.print("%  Temperature: ");
            Serial.print(share_data->temperature);
            Serial.println("°C");

            xSemaphoreGive(share_data->se_data);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}