#include "temp_humi_monitor.h"

LiquidCrystal_I2C lcd(33, 16, 2);

void task_temp_humi_monitor(void *pvParameter)
{
    Wire.begin(11, 12); // 11: GPIO selected SDA in I2C and 12 is GPIO selected SCL in I2C
    Serial.begin(115200);
    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Starting");

    sensor_data_t receivedData = {0};
    system_se_t *sys_se = (system_se_t *)pvParameter;

    while (1)
    {
        if (xQueueReceive(sys_se->queue_raw_data, &receivedData, portMAX_DELAY) == pdPASS)
        {

            // Ngay khi dòng code này chạy, receivedData đã chứa đầy đủ số đo
            Serial.printf("Recieved! TEMP: %.1f, HUMI: %.1f\n",
                          receivedData.temperature, receivedData.humidity);

            // Tiến hành chạy AI đánh giá dữ liệu ở đây...

            if (xSemaphoreTake(sys_se->se_i2c, portMAX_DELAY) == pdTRUE)
            {
                float temp = receivedData.temperature;
                float humi = receivedData.humidity;
                if (temp > 35 || humi < 40)
                {
                    warning_monitor(temp, humi);
                }
                else if (temp < 30 && humi >= 40 && humi <= 70)
                {
                    normal_monitor(temp, humi);
                }
                else
                {
                    alert_monitor(temp, humi);
                }
                xSemaphoreGive(sys_se->se_i2c);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
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
    lcd.write((uint8_t)223); 
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