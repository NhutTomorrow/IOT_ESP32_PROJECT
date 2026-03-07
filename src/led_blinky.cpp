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
  Sensor_data *shared_data = (Sensor_data *)pvParameter;
  pinMode(LED_GPIO, OUTPUT);

  float curTemp = -1;
  while (1)
  {
    if (xSemaphoreTake(shared_data->se_data, (TickType_t)10) == pdTRUE)
    {
      curTemp = shared_data->temperature;
      xSemaphoreGive(shared_data->se_data);
    }
    if (xSemaphoreTake(shared_data->se_normal, 0) == pdTRUE)
    {
      digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
      xSemaphoreGive(shared_data->se_normal);
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    else if (xSemaphoreTake(shared_data->se_warning, TickType_t(0)) == pdTRUE)
    {
      digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
      xSemaphoreGive(shared_data->se_warning);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    else
    {
      if (xSemaphoreTake(shared_data->se_critical, 0) == pdTRUE)
      {
        digitalWrite(LED_GPIO, HIGH);
        xSemaphoreGive(shared_data->se_critical);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  }
}