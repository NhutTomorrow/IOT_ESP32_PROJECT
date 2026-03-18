#include "task_webserver.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Con trỏ sys dùng trong callbacks
static system_se_t *_sys = nullptr;
static String buildSettingsPage()
{
    // Đọc config
    Preferences prefs;
    prefs.begin("config", true);
    String ssid = prefs.getString("ssid", "");
    String token = prefs.getString("token", "");
    prefs.end();

    // Đọc WiFi status
    wifi_status_t wst = WIFI_DISCONNECTED;
    xQueuePeek(_sys->queue_wifi_status, &wst, 0);
    bool online = (wst == WIFI_CONNECTED);

    // Load từ flash vào String
    String html = FPSTR(SETTINGS_HTML);

    // Inject data vào placeholder
    html.replace("{{SSID}}", ssid);
    html.replace("{{TOKEN}}", token);
    html.replace("{{DOT_CLASS}}", online ? "on" : "");
    html.replace("{{STATUS_TEXT}}", online
                                        ? "Online — " + WiFi.localIP().toString()
                                        : "Offline — AP mode");
    html.replace("{{CHIP_CLASS}}", online ? "on" : "");
    html.replace("{{CHIP_TEXT}}", online ? "Online" : "Offline");

    return html;
}
static void onWsEvent(AsyncWebSocket *server,
                      AsyncWebSocketClient *client,
                      AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
    if (_sys == nullptr)
    {
        Serial.printf("sys is null \n");
        return;
    }
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WS client #%u connected\n", client->id());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WS client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        // Parse lệnh từ browser: "led:on", "neo:255,0,0"
        char *buf = (char *)malloc(len + 1);
        if (!buf)
            return;
        memcpy(buf, data, len);
        buf[len] = '\0';
        String msg = String(buf);
        free(buf);
        Serial.println("WS recv: " + msg);

        if (msg.startsWith("led:"))
        {
            String action = msg.substring(4);
            if (action == "auto")
            {
                led_mode_t mode = LED_MODE_AUTO;
                xQueueOverwrite(_sys->queue_led_mode, &mode);
                Serial.printf("SWITCH LED TO AUTO MODE");
            }
            else
            {
                // ✅ Thêm log kiểm tra
                Serial.printf("[DEBUG] queue_led_cmd handle: %p\n", _sys->queue_led_cmd);
                Serial.printf("[DEBUG] queue_led_mode handle: %p\n", _sys->queue_led_mode);

                if (_sys->queue_led_cmd == NULL)
                {
                    Serial.printf("[ERROR] queue_led_cmd is NULL → skip\n");
                    return;
                }
                led_cmd_t cmd;
                cmd.state = (action == "on");
                led_mode_t mode = LED_MODE_MANUAL;
                xQueueOverwrite(_sys->queue_led_mode, &mode);
                xQueueOverwrite(_sys->queue_led_cmd, &cmd);
                Serial.printf("SWITCH LED TO MANUAL MODE");
            }
        }
        else if (msg.startsWith("neo:"))
        {
            String action = msg.substring(4);
            if (action == "auto")
            {
                neo_mode_t mode = NEO_MODE_AUTO;
                xQueueOverwrite(_sys->queue_neo_mode, &mode);
                Serial.printf("SWITCH NEO TO AUTO MODE");
            }
            else
            {
                neo_cmd_t cmd = {0, 0, 0};
                sscanf(msg.substring(4).c_str(), "%hhu,%hhu,%hhu",
                       &cmd.r, &cmd.g, &cmd.b);
                neo_mode_t mode = NEO_MODE_MANUAL;
                xQueueOverwrite(_sys->queue_neo_mode, &mode);
                xQueueOverwrite(_sys->queue_neo_cmd, &cmd);
                Serial.printf("SWITCH NEO TO MANUAL MODE");
            }
        }
    }
}

static void setupRoutes(system_se_t *sys)
{
    // WebSocket
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Trang chính
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
              { req->send(200, "text/html", HTML_PAGE); });

    // Trang settings
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *req)
              { req->send(200, "text/html", buildSettingsPage()); });
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *req)
              {
                  Preferences prefs;
                  prefs.begin("config", false);
                  prefs.clear(); // xóa toàn bộ
                  prefs.end();
                  req->send(200, "text/plain", "OK");
                  delay(500);
                  ESP.restart(); // khởi động lại
              });
    // Nhận WiFi config
    server.on("/connect", HTTP_GET, [sys](AsyncWebServerRequest *req)
              {
        if (!req->hasParam("ssid")) {
            req->send(400, "text/plain", "Missing SSID");
            return;
        }

        wifi_info_t cfg;
        strlcpy(cfg.ssid,  req->getParam("ssid")->value().c_str(),  64);
        strlcpy(cfg.pass,  req->getParam("pass")->value().c_str(),  64);
        strlcpy(cfg.token, req->getParam("token")->value().c_str(), 128);

        // Lưu flash
        Preferences prefs;
        prefs.begin("config", false);
        prefs.putString("ssid",  cfg.ssid);
        prefs.putString("pass",  cfg.pass);
        prefs.putString("token", cfg.token);
        prefs.end();

        // Đẩy queue
        xQueueOverwrite(sys->queue_wifi_config, &cfg);

        req->send(200, "text/plain",
            "⏳ Đang kết nối: " + String(cfg.ssid)); });

    server.begin();
    Serial.println("Async server started");
}

void task_websever(void *pvParameter)
{
    system_se_t *sys = (system_se_t *)pvParameter;
    _sys = sys;

    bool connecting = false;
    uint32_t connectStart = 0;

    // Load config cũ
    Preferences prefs;
    char saved_ssid[64] = "";
    char saved_pass[64] = "";
    prefs.begin("config", true);
    strlcpy(saved_ssid, prefs.getString("ssid", "").c_str(), 64);
    strlcpy(saved_pass, prefs.getString("pass", "").c_str(), 64);
    prefs.end();
    Serial.printf("Load old config from Flash!!!");
    // Khởi động WiFi
    if (strlen(saved_ssid) > 0)
    {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        delay(100);
        WiFi.begin(saved_ssid, saved_pass);
        connecting = true;
        connectStart = millis();
        Serial.printf("INIT WIFI WITH AP AND STA MODE");
        if (WiFi.softAP(AP_SSID, AP_PASSWORD))
        {
            Serial.printf("AP OK - SSID: %s IP: %s\n",
                          AP_SSID,
                          WiFi.softAPIP().toString().c_str());
        }
        else
        {
            Serial.printf("AP FAILED!\n"); // ← nếu in cái này thì vấn đề ở chỗ khác
        }
    }
    else
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        Serial.printf("ACCESS WIFI FAILED");
        Serial.printf("SWITCH TO AP AND ENTER NEW WIFI FROM WEB AT 192.168.4.1");
    }

    setupRoutes(sys);

    wifi_info_t newCfg;
    uint32_t lastPush = 0;

    for (;;)
    {
        // ── 1. Nhận config WiFi mới ──
        if (xQueueReceive(sys->queue_wifi_config, &newCfg, 0) == pdTRUE)
        {
            // WiFi.mode(WIFI_AP_STA);
            WiFi.disconnect();
            WiFi.begin(newCfg.ssid, newCfg.pass);
            connecting = true;
            connectStart = millis();
            Serial.printf("Get new confg wifi from web sever");
        }

        // ── 2. Theo dõi kết nối STA ──
        if (connecting)
        {
            if (WiFi.status() == WL_CONNECTED)
            {

                Serial.println("STA: " + WiFi.localIP().toString());
                wifi_status_t st = WIFI_CONNECTED;
                xQueueOverwrite(sys->queue_wifi_status, &st);
                xSemaphoreGive(sys->se_wifi); // báo taskCloud
                connecting = false;
            }
            else if (millis() - connectStart > 10000)
            {
                WiFi.mode(WIFI_AP);
                WiFi.softAP(AP_SSID, AP_PASSWORD);
                wifi_status_t st = WIFI_FAILED;
                xQueueOverwrite(sys->queue_wifi_status, &st);
                connecting = false;
                Serial.printf("Access continute fail, please enter from web again");
            }
        }

        // ── 3. Push data qua WebSocket mỗi 2 giây ──
        if (millis() - lastPush >= 2000)
        {
            lastPush = millis();

            sensor_data_t data = {0, 0, 0};
            xQueuePeek(sys->queue_raw_data, &data, 0);

            wifi_status_t wst = WIFI_DISCONNECTED;
            xQueuePeek(sys->queue_wifi_status, &wst, 0);

            const char *mlStr = (data.ml_status == 0) ? "Normal" : (data.ml_status == 1) ? "⚠ Warning"
                                                                                         : "🚨 Critical";

            // Build JSON push xuống tất cả browser đang mở
            char json[128];
            snprintf(json, sizeof(json),
                     "{\"temp\":%.1f,\"hum\":%.1f,\"ml\":\"%s\",\"wifi\":%s}",
                     data.temperature,
                     data.humidity,
                     mlStr,
                     (wst == WIFI_CONNECTED) ? "true" : "false");
            ws.textAll(json);    // ← push tới mọi client
            ws.cleanupClients(); // dọn client đã ngắt
        }

        // ── 4. Nút BOOT → về AP ──
        if (digitalRead(BOOT_PIN) == LOW)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (digitalRead(BOOT_PIN) == LOW)
            {
                WiFi.mode(WIFI_AP);
                WiFi.softAP(AP_SSID, AP_PASSWORD);
                wifi_status_t st = WIFI_DISCONNECTED;
                xQueueOverwrite(sys->queue_wifi_status, &st);
                connecting = false;
                Serial.println("BOOT → AP mode");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
