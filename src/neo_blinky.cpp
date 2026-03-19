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
    system_se_t *sys_se = (system_se_t *)pvParameter;

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.setBrightness(10);
    strip.show();

    neo_mode_t mode = NEO_MODE_AUTO;
    uint32_t manualUntil = 0;
    neo_cmd_t cmd;
    Serial.printf("init Task Neo success!!!");
    while (1)
    {
        // ── 1. Cập nhật mode nếu có lệnh từ Web ──
        neo_mode_t newMode;
        if (xQueueReceive(sys_se->queue_neo_mode, &newMode, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            mode = newMode;
            if (mode == NEO_MODE_MANUAL)
                manualUntil = millis() + 30000; // manual 30 giây
        }

        // ── 2. Timeout manual → tự về Auto ──
        if (mode == NEO_MODE_MANUAL && millis() > manualUntil)
        {
            mode = NEO_MODE_AUTO;
            Serial.println("Neo: manual timeout → auto");
        }

        // ── 3. Xử lý theo mode ──
        if (mode == NEO_MODE_MANUAL)
        {
            // Chỉ nghe lệnh màu từ Web
            if (xQueueReceive(sys_se->queue_neo_cmd, &cmd, 0) == pdTRUE)
            {
                strip.setPixelColor(0, strip.Color(cmd.r, cmd.g, cmd.b));
            }
        }
        else // NEO_MODE_AUTO — Task 2 điều khiển theo độ ẩm
        {
            if (xSemaphoreTake(sys_se->se_humi_normal, 0) == pdTRUE)
            {
                strip.setPixelColor(0, strip.Color(0, 255, 0)); // Xanh lá
                Serial.printf("NORMAL NEO HUMIDITY");
            }
            else if (xSemaphoreTake(sys_se->se_humi_warning, 0) == pdTRUE)
            {
                strip.setPixelColor(0, strip.Color(255, 255, 0)); // Vàng
                Serial.printf("WARNING NEO HUMIDITY");
            }
            else if (xSemaphoreTake(sys_se->se_humi_critical, 0) == pdTRUE)
            {
                strip.setPixelColor(0, strip.Color(255, 0, 0)); // Đỏ
                Serial.printf("CRITICAL NEO HUMIDITY");
            }
        }

        strip.show();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
