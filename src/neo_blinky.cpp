#include "neo_blinky.h"

void neo_blinky(void *pvParameters)
{

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();

    while (1)
    {
        strip.setPixelColor(0, strip.Color(255, 0, 0)); // Set pixel 0 to red
        strip.show();                                   // Update the strip

        // Wait for 500 milliseconds
        vTaskDelay(500);

        // Set the pixel to off
        strip.setPixelColor(0, strip.Color(0, 0, 0)); // Turn pixel 0 off
        strip.show();                                 // Update the strip

        // Wait for another 500 milliseconds
        vTaskDelay(500);
    }
}
void task_humi_neo(void *pvParameter)
{
    Sensor_data *shared_data = (Sensor_data *)pvParameter;
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();
    float curHumi = -1;
    while (1)
    {
        if (xSemaphoreTake(shared_data->se_data, 10) == pdTRUE)
        {
            curHumi = shared_data->humidity;
            xSemaphoreGive(shared_data->se_data);
        }

        if (xSemaphoreTake(shared_data->se_normal, (TickType_t)0) == pdTRUE)
        {
            strip.setPixelColor(0, strip.Color(0, 255, 0));
            xSemaphoreGive(shared_data->se_normal);
        }
        else if (xSemaphoreTake(shared_data->se_warning, (TickType_t)0) == pdTRUE)
        {
            strip.setPixelColor(0, strip.Color(0, 0, 255));
            xSemaphoreGive(shared_data->se_warning);
        }
        else
        {
            if (xSemaphoreTake(shared_data->se_critical, (TickType_t)0) == pdTRUE)
            {
                strip.setPixelColor(0, strip.Color(255, 0, 0));

                xSemaphoreGive(shared_data->se_critical);
            }
        }
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}