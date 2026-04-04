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
  system_se_t *sys_se = (system_se_t *)pvParameter;
  led_cmd_t cmd;
  led_mode_t led_mode = LED_MODE_AUTO;
  uint32_t untilmanual = 0;
  pinMode(LED_GPIO, OUTPUT);
  Serial.printf("init task temp blinky success!!!");
  while (1)
  {
    led_mode_t new_mode;
    if (xQueueReceive(sys_se->queue_led_mode, &new_mode, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      led_mode = new_mode;
      if (led_mode == LED_MODE_MANUAL)
      {
        untilmanual = millis() + 30000;
        Serial.printf("SWITCH TO LED MANUAL MODE FROM WEB SEVER\n");
      }
    }
    if (led_mode == LED_MODE_MANUAL && millis() > untilmanual)
    {
      led_mode = LED_MODE_AUTO;
      Serial.printf("SWITCH TO LED AUTO MODE\n");
    }
    if (led_mode == LED_MODE_MANUAL)
    {
      if (xQueueReceive(sys_se->queue_led_cmd, &cmd, 0) == pdTRUE)
      {

        digitalWrite(LED_GPIO, cmd.state ? HIGH : LOW);
        Serial.printf("CONTROL WITH WEB\n");
      }
    }
    else
    {
      if (xSemaphoreTake(sys_se->se_temp_critical,
                         0) == pdTRUE)
      {

        for (int i = 0; i < 5; i++)
        {
          digitalWrite(LED_GPIO, HIGH);
          vTaskDelay(pdMS_TO_TICKS(100));
          digitalWrite(LED_GPIO, LOW);
          vTaskDelay(pdMS_TO_TICKS(100));
        }
        Serial.printf("CRITICAL WARNING TEMPERATURE");
      }
      else if (xSemaphoreTake(sys_se->se_temp_warning,
                              0) == pdTRUE)
      {

        for (int i = 0; i < 4; i++)
        {
          digitalWrite(LED_GPIO, HIGH);
          vTaskDelay(pdMS_TO_TICKS(1000));
          digitalWrite(LED_GPIO, LOW);
          vTaskDelay(pdMS_TO_TICKS(1000));
        }
        Serial.printf("WARNING TEMPERATURE");
      }
      else
      {
        if (xSemaphoreTake(sys_se->se_temp_normal,
                           0) == pdTRUE)
        {
          digitalWrite(LED_GPIO, HIGH);
          Serial.printf("NORMAL TEMPERTURE");
          vTaskDelay(pdMS_TO_TICKS(1000));
        }
      }
    }
  }
}
void task_temp_blink_state(void *pvParameter)
{
  // system_se_t *sys_se = (system_se_t *)pvParameter;

  // // led_cmd_t cmd;
  // led_mode_t led_mode = LED_MODE_AUTO;
  // uint32_t lastBlinkTime = 0;
  // uint32_t untilmanual = 0;
  // uint32_t periodBlink = 1000; // time to blink for led
  // bool pinLedState = false;
  // pinMode(LED_GPIO, OUTPUT);

  // Serial.printf("init task temp blinky success!!!");
  led_mode_t led_mode = INIT_LED_TEMP;
  system_se_t *sys_se = (system_se_t *)pvParameter;
  uint32_t lastBlinkTime = 0;
  uint32_t untilmanual = 0;
  uint32_t periodBlink = 1000; // time to blink for led
  bool pinLedState = false;
  while (1)
  {
    // led_mode_t new_mode;
    // if (xQueueReceive(sys_se->queue_led_mode, &new_mode, 0) == pdTRUE)
    // {
    //   led_mode = new_mode;
    // }
    switch (led_mode)
    {
    case INIT_LED_TEMP:

      // led_cmd_t cmd
      pinMode(LED_GPIO, OUTPUT);

      Serial.printf("init task temp blinky success!!!");
      led_mode = LED_MODE_AUTO;
      break;
    case LED_MODE_AUTO:

      Serial.println("LED MODE AUTO!!");

      if (xSemaphoreTake(sys_se->se_temp_normal, 0) == pdTRUE)
      {

        led_mode = STATE_NORMAL;
      }
      else if (xSemaphoreTake(sys_se->se_temp_warning, 0) == pdTRUE)
      {
        periodBlink = 1000;
        led_mode = STATE_WARNING;
      }
      else if (xSemaphoreTake(sys_se->se_temp_critical, 0) == pdTRUE)
      {
        periodBlink = 100;
        led_mode = STATE_CRITICAL;
      }
      if (xQueueReceive(sys_se->queue_led_mode, &led_mode, 0) == pdTRUE)
      {
        untilmanual = millis() + 30000; // timeout 30s manual -> auto
      }
      break;
    case LED_MODE_MANUAL:
      led_cmd_t cmd;
      if (xQueueReceive(sys_se->queue_led_cmd, &cmd, 0) == pdTRUE)
      {
        digitalWrite(LED_GPIO, cmd.state ? HIGH : LOW);
      }
      Serial.println("LED MODE MANUAL !!!");
      if (millis() > untilmanual)
      {
        led_mode = LED_MODE_AUTO;
      }
      break;
    case STATE_NORMAL:
      digitalWrite(LED_GPIO, HIGH);
      Serial.println("TEMPERATURE NORMAL STATE");
      led_mode = LED_MODE_AUTO;
      xSemaphoreGive(sys_se->se_temp_normal);

      break;
    case STATE_WARNING:
      if (millis() - lastBlinkTime >= periodBlink)
      {
        lastBlinkTime = millis();
        digitalWrite(LED_GPIO, !pinLedState ? HIGH : LOW);
      }
      Serial.println("TEMPERATURE WARNING STATE");
      led_mode = LED_MODE_AUTO;
      xSemaphoreGive(sys_se->se_temp_warning);

      break;
    case STATE_CRITICAL:
      if (millis() - lastBlinkTime >= periodBlink)
      {
        lastBlinkTime = millis();
        digitalWrite(LED_GPIO, !pinLedState ? HIGH : LOW);
      }
      Serial.println("TEMPERATURE CRITICAL STATE");
      led_mode = LED_MODE_AUTO;
      xSemaphoreGive(sys_se->se_temp_critical);

      break;
    default:

      break;
    }
  }
}