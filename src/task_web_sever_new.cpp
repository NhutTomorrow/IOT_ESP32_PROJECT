#include "task_webserver.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Con trỏ sys dùng trong WS callback (async_tcp context)
static system_se_t *_sys = nullptr;

// State machine hiện tại — chỉ đọc/ghi trong task_websever
static web_wifi_t _wifiState = WEB_WIFI_INIT;

// ════════════════════════════════════════════════════════
//  HELPER: Serve file từ LittleFS, fallback về PROGMEM
// ════════════════════════════════════════════════════════
//
//  Ưu tiên:  LittleFS (dễ update UI) → PROGMEM (luôn có sẵn)
//
static void serveMainPage(AsyncWebServerRequest *req)
{
    if (LittleFS.exists("/index.html"))
    {
        // ✅ Cách 1: Serve từ LittleFS — dễ update UI không cần recompile
        req->send(LittleFS, "/index.html", "text/html");
        Serial.println("[WEB] Serve index.html từ LittleFS");
    }
    else
    {
        // ✅ Fallback Cách 2: PROGMEM — luôn hoạt động kể cả khi chưa upload FS
        req->send(200, "text/html", HTML_PAGE_FALLBACK);
        Serial.println("[WEB] LittleFS không có file → dùng PROGMEM fallback");
    }
}

static void serveStaticFile(AsyncWebServerRequest *req,
                            const char *fsPath,
                            const char *contentType)
{
    if (LittleFS.exists(fsPath))
    {
        req->send(LittleFS, fsPath, contentType);
    }
    else
    {
        req->send(404, "text/plain", "File not found in LittleFS");
    }
}

// ════════════════════════════════════════════════════════
//  HELPER: Build trang Settings (luôn dynamic → PROGMEM template)
// ════════════════════════════════════════════════════════
static const char *wifiStateStr(web_wifi_t st)
{
    switch (st)
    {
    case WEB_WIFI_INIT:
        return "Init";
    case WEB_WIFI_AP:
        return "AP Only";
    case WEB_WIFI_CONNECTING:
        return "Connecting";
    case WEB_WIFI_CONNECTED:
        return "Connected";
    case WEB_WIFI_RECONNECTING:
        return "Reconnecting";
    default:
        return "Unknown";
    }
}

static void serveSettingsPage(AsyncWebServerRequest *req)
{
    Preferences prefs;
    prefs.begin("config", true);
    String ssid = prefs.getString("ssid", "");
    String token = prefs.getString("token", "");
    prefs.end();

    bool online = (_wifiState == WEB_WIFI_CONNECTED);

    String html = FPSTR(SETTINGS_HTML);
    html.replace("{{SSID}}", ssid);
    html.replace("{{TOKEN}}", token);
    html.replace("{{WIFI_STATE}}", wifiStateStr(_wifiState));
    html.replace("{{DOT_CLASS}}", online ? "on" : "");
    html.replace("{{STATUS_TEXT}}", online
                                        ? "Online — " + WiFi.localIP().toString()
                                        : "Offline — AP " + WiFi.softAPIP().toString());
    html.replace("{{CHIP_CLASS}}", online ? "on" : "");
    html.replace("{{CHIP_TEXT}}", online ? "Online" : "Offline");

    req->send(200, "text/html", html);
}

// ════════════════════════════════════════════════════════
//  WebSocket Event Handler
// ════════════════════════════════════════════════════════
static void onWsEvent(AsyncWebSocket *server,
                      AsyncWebSocketClient *client,
                      AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
    if (_sys == nullptr)
        return;

    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("[WS] Client #%u connected\n", client->id());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode != WS_TEXT)
            return;

        // Copy an toàn vào buffer
        char *buf = (char *)malloc(len + 1);
        if (!buf)
            return;
        memcpy(buf, data, len);
        buf[len] = '\0';
        String msg = String(buf);
        free(buf);

        Serial.println("[WS] recv: " + msg);

        // ── Parse lệnh LED ──
        if (msg.startsWith("led:"))
        {
            String action = msg.substring(4);
            led_mode_t mode = (action == "auto") ? LED_MODE_AUTO : LED_MODE_MANUAL;
            xQueueOverwrite(_sys->queue_led_mode, &mode);

            if (mode == LED_MODE_MANUAL)
            {
                led_cmd_t cmd = {.state = (action == "on")};
                xQueueOverwrite(_sys->queue_led_cmd, &cmd);
            }
            Serial.printf("[WS] LED → %s\n", action.c_str());
        }
        // ── Parse lệnh NeoPixel ──
        else if (msg.startsWith("neo:"))
        {
            String action = msg.substring(4);
            neo_mode_t mode = (action == "auto") ? NEO_MODE_AUTO : NEO_MODE_MANUAL;
            xQueueOverwrite(_sys->queue_neo_mode, &mode);

            if (mode == NEO_MODE_MANUAL)
            {
                neo_cmd_t cmd = {0, 0, 0};
                sscanf(action.c_str(), "%hhu,%hhu,%hhu", &cmd.r, &cmd.g, &cmd.b);
                xQueueOverwrite(_sys->queue_neo_cmd, &cmd);
            }
            Serial.printf("[WS] NEO → %s\n", action.c_str());
        }
    }
}

// ════════════════════════════════════════════════════════
//  Setup Routes
// ════════════════════════════════════════════════════════
static void setupRoutes(system_se_t *sys)
{
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // ── Trang chính: LittleFS → PROGMEM fallback ──
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
              { serveMainPage(req); });

    // ── Static files từ LittleFS (CSS/JS riêng khi dùng LittleFS) ──
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *req)
              { serveStaticFile(req, "/script.js", "application/javascript"); });
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *req)
              { serveStaticFile(req, "/styles.css", "text/css"); });

    // ── Settings page: luôn dùng PROGMEM template (cần inject data) ──
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *req)
              { serveSettingsPage(req); });

    // ── Connect: ✅ POST thay GET — password không lộ trên URL ──
    server.on("/connect", HTTP_GET,
              [sys](AsyncWebServerRequest *req)
              {
                  if (!req->hasParam("ssid"))
                  {
                      req->send(400, "text/plain", "Missing SSID");
                      return;
                  }

                  wifi_info_t cfg;
                  strlcpy(cfg.ssid, req->getParam("ssid")->value().c_str(), 64);
                  strlcpy(cfg.pass, req->getParam("pass")->value().c_str(), 64);
                  strlcpy(cfg.token, req->getParam("token")->value().c_str(), 128);

                  Preferences prefs;
                  prefs.begin("config", false);
                  prefs.putString("ssid", cfg.ssid);
                  prefs.putString("pass", cfg.pass);
                  prefs.putString("token", cfg.token);
                  prefs.end();

                  xQueueOverwrite(sys->queue_wifi_config, &cfg);
                  req->send(200, "text/plain", "⏳ Đang kết nối: " + String(cfg.ssid));
              });

    // ── Reset: xóa NVS rồi restart an toàn (không block async_tcp) ──
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        Preferences prefs;
        prefs.begin("config", false);
        prefs.clear();
        prefs.end();
        req->send(200, "text/plain", "OK — restarting...");

        // Tạo task riêng để restart — không block async_tcp
        xTaskCreate(
            [](void *) { vTaskDelay(pdMS_TO_TICKS(500)); ESP.restart(); },
            "rst_task", 1024, nullptr, 5, nullptr
        ); });

    // ── API: trạng thái WiFi state machine (debug) ──
    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        String json = "{\"state\":\"";
        json += wifiStateStr(_wifiState);
        json += "\",\"ip\":\"";
        json += (_wifiState == WEB_WIFI_CONNECTED)
                    ? WiFi.localIP().toString()
                    : WiFi.softAPIP().toString();
        json += "\"}";
        req->send(200, "application/json", json); });

    server.begin();
    Serial.println("[WEB] Async server started on port 80");
}

// ════════════════════════════════════════════════════════
//  State Machine Handlers
// ════════════════════════════════════════════════════════

// ── STATE_INIT ──
// Đọc config từ NVS, quyết định bước tiếp theo
static void handleInit(char *out_ssid, char *out_pass)
{
    Preferences prefs;
    prefs.begin("config", true);
    strlcpy(out_ssid, prefs.getString("ssid", "").c_str(), 64);
    strlcpy(out_pass, prefs.getString("pass", "").c_str(), 64);
    prefs.end();

    Serial.printf("[SM] INIT → ssid='%s'\n", out_ssid);

    // Luôn bật AP để user có thể vào cài đặt bất cứ lúc nào
    WiFi.mode(strlen(out_ssid) > 0 ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[SM] AP ready: %s / %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ── STATE_AP_ONLY ──
// Chờ user gửi config WiFi mới qua web
static web_wifi_t handleApOnly(system_se_t *sys, wifi_info_t *out_cfg)
{
    if (xQueueReceive(sys->queue_wifi_config, out_cfg, 0) == pdTRUE)
    {
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(out_cfg->ssid, out_cfg->pass);
        Serial.printf("[SM] AP_ONLY → CONNECTING '%s'\n", out_cfg->ssid);
        Serial.printf("Get new confg wifi from web sever");
        return WEB_WIFI_CONNECTING;
    }
    return WEB_WIFI_AP; // giữ nguyên
}

// ── STATE_CONNECTING ──
// Đợi kết nối STA, timeout → về AP_ONLY
static web_wifi_t handleConnecting(system_se_t *sys, uint32_t startMs)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[SM] CONNECTING → CONNECTED (%s)\n",
                      WiFi.localIP().toString().c_str());

        wifi_status_t st = WIFI_CONNECTED;
        xQueueOverwrite(sys->queue_wifi_status, &st);
        xSemaphoreGive(sys->se_wifi); // báo task_cloud
        return WEB_WIFI_CONNECTED;
    }

    if (millis() - startMs > STA_TIMEOUT_MS)
    {
        Serial.println("[SM] CONNECTING → AP_ONLY (timeout)");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD);

        wifi_status_t st = WIFI_FAILED;
        xQueueOverwrite(sys->queue_wifi_status, &st);
        return WEB_WIFI_AP;
    }

    return WEB_WIFI_CONNECTING; // giữ nguyên, tiếp tục chờ
}

// ── STATE_CONNECTED ──
// Theo dõi kết nối, nhận config mới, push WebSocket data
static web_wifi_t handleConnected(system_se_t *sys,
                                  uint32_t *lastPush,
                                  wifi_info_t *out_newCfg)
{
    // Mất kết nối → thử lại
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[SM] CONNECTED → RECONNECTING");
        wifi_status_t st = WIFI_DISCONNECTED;
        xQueueOverwrite(sys->queue_wifi_status, &st);
        WiFi.reconnect();
        return WEB_WIFI_RECONNECTING;
    }

    // Nhận config WiFi mới từ web
    if (xQueueReceive(sys->queue_wifi_config, out_newCfg, 0) == pdTRUE)
    {
        WiFi.disconnect();
        WiFi.begin(out_newCfg->ssid, out_newCfg->pass);
        Serial.printf("[SM] CONNECTED → CONNECTING (new cfg: %s)\n", out_newCfg->ssid);
        return WEB_WIFI_CONNECTING;
    }

    // Push WebSocket data định kỳ
    if (millis() - *lastPush >= WS_PUSH_INTERVAL_MS)
    {
        *lastPush = millis();

        sensor_data_t data = {0, 0, 0};
        xQueuePeek(sys->queue_raw_data, &data, 0);

        const char *mlStr = (data.ml_status == 0)   ? "Normal"
                            : (data.ml_status == 1) ? "⚠ Warning"
                                                    : "🚨 Critical";
        char json[128];
        snprintf(json, sizeof(json),
                 "{\"temp\":%.1f,\"hum\":%.1f,\"ml\":\"%s\",\"wifi\":true}",
                 data.temperature, data.humidity, mlStr);
        ws.textAll(json);
        ws.cleanupClients();
    }

    return WEB_WIFI_CONNECTED;
}

// ── STATE_RECONNECTING ──
// Tự động thử lại, timeout → về AP_ONLY
static web_wifi_t handleReconnecting(system_se_t *sys, uint32_t startMs)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[SM] RECONNECTING → CONNECTED (%s)\n",
                      WiFi.localIP().toString().c_str());

        wifi_status_t st = WIFI_CONNECTED;
        xQueueOverwrite(sys->queue_wifi_status, &st);
        xSemaphoreGive(sys->se_wifi);
        return WEB_WIFI_CONNECTED;
    }

    if (millis() - startMs > STA_TIMEOUT_MS)
    {
        Serial.println("[SM] RECONNECTING → AP_ONLY (give up)");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD);

        wifi_status_t st = WIFI_FAILED;
        xQueueOverwrite(sys->queue_wifi_status, &st);
        return WEB_WIFI_AP;
    }

    return WEB_WIFI_RECONNECTING;
}

// ════════════════════════════════════════════════════════
//  Main Task
// ════════════════════════════════════════════════════════
void task_websever_new(void *pvParameter)
{
    system_se_t *sys = (system_se_t *)pvParameter;
    _sys = sys;

    // Khởi động LittleFS (thử — không crash nếu chưa có)
    if (!LittleFS.begin(false))
    {
        Serial.println("[FS] LittleFS mount failed → sẽ dùng PROGMEM fallback");
    }
    else
    {
        Serial.println("[FS] LittleFS mounted OK");
        Serial.printf("[FS]  index.html: %s\n", LittleFS.exists("/index.html") ? "✅" : "❌");
        Serial.printf("[FS]  script.js : %s\n", LittleFS.exists("/script.js") ? "✅" : "❌");
        Serial.printf("[FS]  styles.css: %s\n", LittleFS.exists("/styles.css") ? "✅" : "❌");
    }

    // Biến phụ trợ state machine
    char saved_ssid[64] = "";
    char saved_pass[64] = "";
    wifi_info_t newCfg = {};
    uint32_t stateStartMs = millis();
    uint32_t lastPush = 0;

    _wifiState = WEB_WIFI_INIT;

    for (;;)
    {
        web_wifi_t nextState = _wifiState;

        switch (_wifiState)
        {
        case WEB_WIFI_INIT:
            handleInit(saved_ssid, saved_pass);
            if (strlen(saved_ssid) > 0)
            {
                WiFi.begin(saved_ssid, saved_pass);
                Serial.printf("[SM] INIT → CONNECTING '%s'\n", saved_ssid);
                nextState = WEB_WIFI_CONNECTING;
            }
            else
            {
                Serial.println("[SM] INIT → AP_ONLY (chưa có config)");
                nextState = WEB_WIFI_AP;
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // đợi AP ổn định
            // ──  server.begin() — an toàn sau WiFi init ─
            setupRoutes(sys);
            if (nextState != _wifiState)
                stateStartMs = millis();
            break;

        case WEB_WIFI_AP:

            if (xQueueReceive(sys->queue_wifi_config, &newCfg, 0) == pdTRUE)
            {
                WiFi.mode(WIFI_AP_STA);
                WiFi.begin(newCfg.ssid, newCfg.pass);
                Serial.printf("[SM] AP_ONLY → CONNECTING '%s'\n", newCfg.ssid);
                Serial.printf("Get new confg wifi from web sever");
                nextState = WEB_WIFI_CONNECTING;
            }

            if (nextState != _wifiState)
                stateStartMs = millis();

            break;

        case WEB_WIFI_CONNECTING:

            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.printf("[SM] CONNECTING → CONNECTED (%s)\n",
                              WiFi.localIP().toString().c_str());

                wifi_status_t st = WIFI_CONNECTED;
                xQueueOverwrite(sys->queue_wifi_status, &st);
                xSemaphoreGive(sys->se_wifi); // báo task_cloud
                nextState = WEB_WIFI_CONNECTED;
            }

            if (millis() - stateStartMs > STA_TIMEOUT_MS)
            {
                Serial.println("[SM] CONNECTING → AP_ONLY (timeout)");
                WiFi.mode(WIFI_AP);
                WiFi.softAP(AP_SSID, AP_PASSWORD);

                wifi_status_t st = WIFI_FAILED;
                xQueueOverwrite(sys->queue_wifi_status, &st);
                nextState = WEB_WIFI_AP;
            }

            if (nextState != _wifiState)
                stateStartMs = millis();
            break;

        case WEB_WIFI_CONNECTED:
            nextState = handleConnected(sys, &lastPush, &newCfg);
            if (nextState != _wifiState)
                stateStartMs = millis();
            break;

        case WEB_WIFI_RECONNECTING:
            nextState = handleReconnecting(sys, stateStartMs);
            if (nextState != _wifiState)
                stateStartMs = millis();
            break;
        default:
            _wifiState = WEB_WIFI_INIT;
            break;
        }
        _wifiState = nextState;
        // ── Nút BOOT → ép về AP_ONLY ──
        if (digitalRead(BOOT_PIN) == LOW)
        {
            vTaskDelay(pdMS_TO_TICKS(50)); // debounce
            if (digitalRead(BOOT_PIN) == LOW)
            {
                Serial.println("[SM] BOOT button → AP_ONLY");
                WiFi.mode(WIFI_AP);
                WiFi.softAP(AP_SSID, AP_PASSWORD);

                wifi_status_t st = WIFI_DISCONNECTED;
                xQueueOverwrite(sys->queue_wifi_status, &st);

                _wifiState = WEB_WIFI_AP;
                stateStartMs = millis();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}