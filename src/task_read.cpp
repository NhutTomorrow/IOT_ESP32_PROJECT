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
        float temp = dht20.getTemperature();
        float humi = dht20.getHumidity();

        if (isnan(temp) || isnan(humi))
        {
            Serial.println("Failed to read from DHT sensor!");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        if (xSemaphoreTake(share_data->se_data, (TickType_t)100) == pdTRUE)
        {
            share_data->humidity = temp;
            share_data->temperature = humi;
            xSemaphoreGive(share_data->se_data);
        }

        if (share_data->humidity > 80.0 || share_data->temperature > 40.0)
        {
            xSemaphoreGive(share_data->se_critical); // alert for other task
        }
        else if (share_data->humidity > 60 || share_data->temperature > 30)
        {
            xSemaphoreGive(share_data->se_warning);
        }
        else
        {
            xSemaphoreGive(share_data->se_normal);
        }

        // write done -> return token critical section
        Serial.print("Humidity: ");
        Serial.print(share_data->humidity);
        Serial.print("%  Temperature: ");
        Serial.print(share_data->temperature);
        Serial.println("°C");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}