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
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();
    system_se_t *sys_se = (system_se_t *)pvParameter;
    while (1)
    {
        if (xSemaphoreTake(sys_se->se_humi_normal, (TickType_t)0) == pdTRUE)
        {
            strip.setPixelColor(0, strip.Color(0, 255, 0));
            xSemaphoreGive(sys_se->se_humi_normal);
        }
        else if (xSemaphoreTake(sys_se->se_humi_warning, (TickType_t)0) == pdTRUE)
        {
            strip.setPixelColor(0, strip.Color(255, 255, 0));
            xSemaphoreGive(sys_se->se_humi_warning);
        }
        else
        {
            if (xSemaphoreTake(sys_se->se_humi_critical, (TickType_t)0) == pdTRUE)
            {
                strip.setPixelColor(0, strip.Color(255, 0, 0));

                xSemaphoreGive(sys_se->se_humi_critical);
            }
        }
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}