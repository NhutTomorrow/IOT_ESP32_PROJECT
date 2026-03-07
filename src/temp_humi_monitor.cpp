#include "temp_humi_monitor.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(33, 16, 2);

void temp_humi_monitor(void *pvParameters)
{

    Wire.begin(11, 12); // 11: GPIO selected SDA in I2C and 12 is GPIO selected SCL in I2C
    Serial.begin(115200);
    dht20.begin();

    while (1)
    {
        /* code */

        dht20.read();
        // Reading temperature in Celsius
        float temperature = dht20.getTemperature();
        // Reading humidity
        float humidity = dht20.getHumidity();

        // Check if any reads failed and exit early
        if (isnan(temperature) || isnan(humidity))
        {
            Serial.println("Failed to read from DHT sensor!");
            temperature = humidity = -1;
            // return;
        }

        // Update global variables for temperature and humidity
        glob_temperature = temperature;
        glob_humidity = humidity;

        // Print the results

        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");

        vTaskDelay(5000);
    }
}

void task_temp_humi_monitor(void *pvParameter)
{
    Wire.begin(11, 12); // 11: GPIO selected SDA in I2C and 12 is GPIO selected SCL in I2C
    Serial.begin(115200);
    dht20.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Starting");
    Sensor_data *shared_data = (Sensor_data *)pvParameter;
    float temp = -1;
    float humi = -1;

    while (1)
    {
        if (xSemaphoreTake(shared_data->se_data, TickType_t(10)) == pdTRUE)
        {
            temp = shared_data->temperature;
            humi = shared_data->humidity;
            xSemaphoreGive(shared_data->se_data);
        }
        if (xSemaphoreTake(shared_data->se_normal, portMAX_DELAY) == pdTRUE)
        {
            // display STATUS: NORMAL and display value
            normal_monitor(temp, humi);
            xSemaphoreGive(shared_data->se_normal);
        }
        else if (xSemaphoreTake(shared_data->se_warning, portMAX_DELAY) == pdTRUE)
        {
            // display STATUS: CHECK AC and display value
            alert_monitor(temp, humi);
            xSemaphoreGive(shared_data->se_critical);
        }
        else
        {
            if (xSemaphoreTake(shared_data->se_warning, portMAX_DELAY) == pdTRUE)
            {
                // display CRITICAL: OVERHEAT!
                warning_monitor(temp, humi);
                xSemaphoreGive(shared_data->se_warning);
            }
        }
    }
}
void normal_monitor(float temp, float humi)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("STATUS: NORMAL");
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C H:");
    lcd.print(humi, 1);
    lcd.print("%");
}
void alert_monitor(float temp, float humi)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WRN: CHECK AC");
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C H:");
    lcd.print(humi, 1);
    lcd.print("%");
}
void warning_monitor(float temp, float humi)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CRITICAL ERROR!!");
    lcd.setCursor(0, 1);
    lcd.print("TEMP OVERLOAD!  ");
}