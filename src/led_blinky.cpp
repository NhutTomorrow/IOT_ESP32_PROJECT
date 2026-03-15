#include "led_blinky.h"

void led_blinky(void *pvParameters)
{
  pinMode(LED_GPIO, OUTPUT);

  while (1)
  {
    digitalWrite(LED_GPIO, HIGH); // turn the LED ON
    vTaskDelay(1000);
    digitalWrite(LED_GPIO, LOW); // turn the LED OFF
    vTaskDelay(1000);
  }
}
void task_temp_blink(void *pvParameter)
{
  sensor_data_t shared_data;
  system_se_t *sys_se = (system_se_t *)pvParameter;
  pinMode(LED_GPIO, OUTPUT);

  while (1)
  {

    if (xSemaphoreTake(sys_se->se_temp_normal, 0) == pdTRUE)
    {
      digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
      xSemaphoreGive(sys_se->se_temp_normal);
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    else if (xSemaphoreTake(sys_se->se_temp_warning, TickType_t(0)) == pdTRUE)
    {
      digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
      xSemaphoreGive(sys_se->se_temp_warning);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    else
    {
      if (xSemaphoreTake(sys_se->se_temp_critical, 0) == pdTRUE)
      {
        digitalWrite(LED_GPIO, HIGH);
        xSemaphoreGive(sys_se->se_temp_critical);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  }
}