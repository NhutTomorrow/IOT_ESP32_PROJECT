#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
// #include "tinyml.h"
#include "task_read.h"
#include "task_webserver.h"
#include "task_core_iot.h"
system_se_t sys;
void init_system_objects(system_se_t *system)
{
  // ════════════════════════════════
  //  BINARY SEMAPHORE — Task 1 (LED + Nhiệt độ)
  // ════════════════════════════════
  system->se_temp_normal = xSemaphoreCreateBinary();
  system->se_temp_warning = xSemaphoreCreateBinary();
  system->se_temp_critical = xSemaphoreCreateBinary();

  // ════════════════════════════════
  //  BINARY SEMAPHORE — Task 2 (NeoPixel + Độ ẩm)
  // ════════════════════════════════
  system->se_humi_normal = xSemaphoreCreateBinary();
  system->se_humi_warning = xSemaphoreCreateBinary();
  system->se_humi_critical = xSemaphoreCreateBinary();

  // ════════════════════════════════
  //  MUTEX — Task 3 (LCD Display)
  // ════════════════════════════════
  system->se_i2c = xSemaphoreCreateMutex();

  // ════════════════════════════════
  //  BINARY SEMAPHORE — Task 4, 6
  // ════════════════════════════════
  system->se_wifi = xSemaphoreCreateBinary();
  // taskWeb Give khi WiFi connected
  // taskCloud Take để bắt đầu MQTT

  // ════════════════════════════════
  //  BINARY SEMAPHORE — Task 5 (TinyML)
  // ════════════════════════════════
  system->se_ml_ready = xSemaphoreCreateBinary();
  // taskML Give khi inference xong
  // taskLCD, taskCloud Take để cập nhật kết quả

  // ════════════════════════════════
  //  QUEUE — Sensor data
  // ════════════════════════════════
  system->queue_raw_data = xQueueCreate(1, sizeof(sensor_data_t));
  // size 1 + xQueueOverwrite → luôn giữ giá trị mới nhất

  system->queue_publish_data = xQueueCreate(5, sizeof(sensor_data_t));
  // size 5 → buffer nếu taskCloud bận

  // ════════════════════════════════
  //  QUEUE — Task 4 (Web điều khiển)
  // ════════════════════════════════
  system->queue_led_cmd = xQueueCreate(1, sizeof(led_cmd_t));
  system->queue_neo_cmd = xQueueCreate(1, sizeof(neo_cmd_t));
  system->queue_led_mode = xQueueCreate(1, sizeof(led_mode_t));
  system->queue_neo_mode = xQueueCreate(1, sizeof(neo_mode_t));
  system->queue_wifi_config = xQueueCreate(1, sizeof(wifi_config_t));
  system->queue_wifi_status = xQueueCreate(1, sizeof(wifi_status_t));

  // ════════════════════════════════
  //  QUEUE — Task 5 (ML result)
  // ════════════════════════════════
  // system->queue_ml_result = xQueueCreate(1, sizeof(ml_result_t));

  // ════════════════════════════════
  //  QUEUE — Task 6 (Cloud token)
  // ════════════════════════════════
  // system->queue_new_token = xQueueCreate(1, sizeof(cloud_config_t));

  // ════════════════════════════════
  //  Kiểm tra tất cả đã tạo thành công
  // ════════════════════════════════
  bool ok = true;

  // Semaphores
  ok &= (system->se_temp_normal != NULL);
  ok &= (system->se_temp_warning != NULL);
  ok &= (system->se_temp_critical != NULL);
  ok &= (system->se_humi_normal != NULL);
  ok &= (system->se_humi_warning != NULL);
  ok &= (system->se_humi_critical != NULL);
  ok &= (system->se_i2c != NULL);
  ok &= (system->se_wifi != NULL);
  ok &= (system->se_ml_ready != NULL);

  // Queues
  ok &= (system->queue_raw_data != NULL);
  ok &= (system->queue_publish_data != NULL);
  ok &= (system->queue_led_cmd != NULL);
  ok &= (system->queue_neo_cmd != NULL);
  ok &= (system->queue_led_mode != NULL);
  ok &= (system->queue_neo_mode != NULL);
  ok &= (system->queue_wifi_config != NULL);
  ok &= (system->queue_wifi_status != NULL);

  // ok &= (system->queue_new_token != NULL);

  if (ok)
    Serial.println("[SYS] All objects created OK");
  else
  {
    Serial.println("[SYS] ERROR: object creation failed!");
    // Dừng hẳn — không chạy tiếp nếu thiếu object
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

void setup()
{
  Serial.begin(115200);
  // pinMode(BOOT_PIN, INPUT_PULLUP);
  // digitalWrite(BOOT_PIN, 0);
  // thêm tạm để xóa bộ nhớ NVS
  // Preferences prefs;
  // prefs.begin("gw", false);
  // prefs.clear();
  // prefs.end();
  // Serial.println("NVS cache cleared!");
  init_system_objects(&sys);
  init_task_read();

  xTaskCreate(task_read_sensor, "Sensor", 8192, &sys, 3, NULL);
  xTaskCreate(task_websever_new, "Web", 8192, &sys, 2, NULL);
  xTaskCreate(task_temp_blink, "LED", 2048, &sys, 2, NULL);
  xTaskCreate(task_humi_neo, "Neo", 2048, &sys, 2, NULL);
  // xTaskCreate(task_coreiot, "Cloud", 4096, &sys, 1, NULL);
  // xTaskCreate(task_temp_humi_monitor, "Monitor", 4096, &sys, 2, NULL);
}

void loop()
{
  // if (check_info_File(1))
  // {
  //   if (!Wifi_reconnect())
  //   {
  //     Webserver_stop();
  //   }
  //   else
  //   {
  //     // CORE_IOT_reconnect();
  //   }
  // }
  // Webserver_reconnect();
  // digitalWrite(LED_GPIO, HIGH); // turn the LED ON
  // vTaskDelay(5000);
  // digitalWrite(LED_GPIO, LOW); // turn the LED OFF
  // vTaskDelay(5000);
}