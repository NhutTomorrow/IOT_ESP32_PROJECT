
#ifndef __TASK_WEBSERVER_H__
#define __TASK_WEBSERVER_H__

#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "WiFi.h"
#include "AsyncTCP.h"
#include "global.h"

#define AP_SSID "SMART_ESP32_WIFI"
#define AP_PASSWORD "12345678"
#define AP_IP IPAddress(192, 168, 4, 1)
#define BOOT_PIN 0

extern AsyncWebServer server;
extern AsyncWebSocket ws;

static const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Sensor Hub</title>
  <style>
    * { box-sizing:border-box; margin:0; padding:0; }
    body { font-family:Arial; background:#0f0f1a; color:#eee; padding:20px; }
    h1   { text-align:center; color:#e94560; margin-bottom:20px; }
    .grid {
      display:grid; grid-template-columns:1fr 1fr;
      gap:12px; max-width:480px; margin:0 auto;
    }
    .card {
      background:#16213e; border-radius:12px;
      padding:16px; grid-column:span 2;
    }
    .card.half { grid-column:span 1; }
    .card h2   { font-size:.9em; color:#a0c4ff; margin-bottom:10px; }
    .value     { font-size:1.8em; font-weight:bold; color:#00ffcc; }
    .badge     { padding:3px 10px; border-radius:20px; font-size:.85em; font-weight:bold; }
    .online    { background:#2ecc71; color:#000; }
    .offline   { background:#e74c3c; color:#fff; }
    button {
      width:48%; padding:10px; margin:1%;
      border:none; border-radius:8px;
      font-size:.9em; font-weight:bold; cursor:pointer;
    }
    .btn-on    { background:#2ecc71; color:#000; }
    .btn-off   { background:#e74c3c; color:#fff; }
    .btn-blue  { background:#3498db; color:#fff; }
    .btn-neo   { background:#9b59b6; color:#fff; }
    .btn-cfg   { width:100%; background:#2c3e50; color:#fff; margin-top:4px; }
    #status-dot {
      display:inline-block; width:10px; height:10px;
      border-radius:50%; background:#e74c3c;
      margin-right:6px; transition: background .5s;
    }
    #status-dot.connected { background:#2ecc71; }
  </style>
</head>
<body>
  <h1>🌡 Smart Sensor Hub</h1>
  <div class="grid">

    <div class="card half">
      <h2>🌡 Nhiệt độ</h2>
      <div class="value"><span id="temp">--</span> °C</div>
    </div>
    <div class="card half">
      <h2>💧 Độ ẩm</h2>
      <div class="value"><span id="hum">--</span> %</div>
    </div>

    <div class="card half">
      <h2>📶 CoreIOT</h2>
      <span id="wifi-badge" class="badge offline">Offline ❌</span>
    </div>
    <div class="card half">
      <h2>🤖 ML Status</h2>
      <div class="value" id="ml" style="font-size:1em">--</div>
    </div>

    <div class="card">
      <h2>💡 LED Indicator</h2>
      <button class="btn-on"  onclick="send('led:on')">Bật LED</button>
      <button class="btn-off" onclick="send('led:off')">Tắt LED</button>
      <button class="btn-cfg" style="margin-top:8px"
              onclick="send('led:auto')">⟳ Auto (Task 1)</button>
    </div>

    <div class="card">
      <h2>🌈 NeoPixel RGB</h2>
      <button class="btn-off"  onclick="send('neo:255,0,0')">🔴 Đỏ</button>
      <button class="btn-on"   onclick="send('neo:0,255,0')">🟢 Xanh</button>
      <button class="btn-blue" onclick="send('neo:0,0,255')">🔵 Lam</button>
      <button class="btn-off"  onclick="send('neo:0,0,0')">⚫ Tắt</button>
      <button class="btn-cfg"  style="margin-top:8px"
              onclick="send('neo:auto')">⟳ Auto (Task 2)</button>
    </div>
    
    <div class="card">
      <span id="status-dot"></span>
      <small id="ws-status">Đang kết nối...</small>
      <button class="btn-cfg" onclick="window.location='/settings'">
        ⚙️ Cài đặt WiFi & Token
      </button>
    </div>
  </div>

  <script>
    let ws;

    function initWS() {
      ws = new WebSocket('ws://' + location.hostname + '/ws');

      ws.onopen = () => {
        document.getElementById('ws-status').innerText  = 'Đã kết nối';
        document.getElementById('status-dot').classList.add('connected');
      };

      // ── Nhận data từ ESP32 ──
      ws.onmessage = (e) => {
        try {
          const d = JSON.parse(e.data);
          if (d.temp !== undefined)
            document.getElementById('temp').innerText = d.temp;
          if (d.hum  !== undefined)
            document.getElementById('hum').innerText  = d.hum;
          if (d.ml   !== undefined)
            document.getElementById('ml').innerText   = d.ml;
          if (d.wifi !== undefined) {
            let badge = document.getElementById('wifi-badge');
            badge.className = 'badge ' + (d.wifi ? 'online' : 'offline');
            badge.innerText = d.wifi ? 'Online ✅' : 'Offline ❌';
          }
        } catch(err) {}
      };

      ws.onclose = () => {
        document.getElementById('ws-status').innerText = 'Mất kết nối — thử lại...';
        document.getElementById('status-dot').classList.remove('connected');
        // Tự reconnect sau 2s
        setTimeout(initWS, 2000);
      };
    }

    // ── Gửi lệnh lên ESP32 qua WebSocket ──
    function send(msg) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(msg);
      }
    }

    initWS();
  </script>
</body>
</html>
)rawhtml";
static const char SETTINGS_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings</title>
<style>
:root{
  --bg:#0d0d1a;--card:#1a1a35;--accent:#6c63ff;
  --accent2:#00d4aa;--danger:#ff4e6a;
  --text:#e8e8f0;--muted:#7b7b9a;
  --border:rgba(255,255,255,0.07);--radius:14px;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);font-family:'Courier New',monospace;
     color:var(--text);min-height:100vh}
.page{max-width:460px;margin:0 auto;padding:24px 16px 40px}
.header{text-align:center;margin-bottom:28px}
.tag{display:inline-block;font-size:10px;letter-spacing:3px;
     text-transform:uppercase;color:var(--accent2);
     border:1px solid var(--accent2);padding:3px 12px;
     border-radius:20px;margin-bottom:10px;opacity:.8}
h1{font-size:22px;font-weight:700;letter-spacing:-.5px}
.sub{font-size:12px;color:var(--muted);margin-top:6px}
.section{margin-bottom:20px}
.label{font-size:10px;letter-spacing:2px;text-transform:uppercase;
       color:var(--muted);margin-bottom:10px;
       display:flex;align-items:center;gap:8px}
.label::after{content:'';flex:1;height:1px;background:var(--border)}
.card{background:var(--card);border:1px solid var(--border);
      border-radius:var(--radius);overflow:hidden}
.field{padding:14px 16px;border-bottom:1px solid var(--border);position:relative}
.field:last-child{border-bottom:none}
.fl{font-size:10px;letter-spacing:1.5px;text-transform:uppercase;
    color:var(--muted);margin-bottom:6px}
.field input{width:100%;background:#0d0d1a;border:1px solid var(--border);
             border-radius:8px;color:var(--text);
             font-family:'Courier New',monospace;
             font-size:13px;padding:9px 12px;outline:none;
             transition:border-color .2s}
.field input:focus{border-color:var(--accent)}
.field input::placeholder{color:var(--muted);font-size:12px}
.token{font-size:11px!important;letter-spacing:1px!important}
.eye{position:absolute;right:26px;bottom:23px;background:none;
     border:none;cursor:pointer;color:var(--muted);font-size:14px}
.sbar{background:var(--card);border:1px solid var(--border);
      border-radius:var(--radius);padding:12px 16px;
      display:flex;align-items:center;gap:12px;margin-bottom:20px}
.dot{width:8px;height:8px;border-radius:50%;flex-shrink:0;
     background:var(--danger);box-shadow:0 0 0 3px rgba(255,78,106,.15)}
.dot.on{background:var(--accent2);box-shadow:0 0 0 3px rgba(0,212,170,.15)}
.si{flex:1}
.st{font-size:12px;font-weight:600}
.ss{font-size:10px;color:var(--muted);margin-top:2px}
.chip{font-size:10px;padding:3px 10px;border-radius:20px;
      background:rgba(255,78,106,.12);color:var(--danger);
      border:1px solid rgba(255,78,106,.25)}
.chip.on{background:rgba(0,212,170,.12);color:var(--accent2);
         border:1px solid rgba(0,212,170,.25)}
.trow{display:flex;align-items:center;justify-content:space-between;
      padding:14px 16px;border-bottom:1px solid var(--border)}
.trow:last-child{border-bottom:none}
.tl{font-size:12px}
.ts{font-size:10px;color:var(--muted);margin-top:2px}
.tog{width:38px;height:20px;border-radius:10px;background:var(--accent);
     position:relative;cursor:pointer;flex-shrink:0}
.tog::after{content:'';position:absolute;width:14px;height:14px;
            border-radius:50%;background:#fff;top:3px;left:21px;transition:left .2s}
.tog.off{background:rgba(255,255,255,.1)}
.tog.off::after{left:3px}
.bgrp{display:flex;gap:10px;margin-top:20px}
.btn{flex:1;padding:13px;border:none;border-radius:12px;
     font-family:'Courier New',monospace;font-size:13px;
     font-weight:700;cursor:pointer;transition:opacity .2s,transform .1s}
.btn:active{transform:scale(.97)}
.bp{background:var(--accent);color:#fff}
.bp:disabled{opacity:.5;cursor:not-allowed}
.bs{background:var(--card);color:var(--muted);border:1px solid var(--border)}
#msg{margin-top:14px;padding:10px 14px;border-radius:10px;
     font-size:12px;text-align:center;display:none}
#msg.ok{background:rgba(0,212,170,.1);color:var(--accent2);
        border:1px solid rgba(0,212,170,.2)}
#msg.err{background:rgba(255,78,106,.1);color:var(--danger);
         border:1px solid rgba(255,78,106,.2)}
.rzone{border:1px dashed rgba(255,78,106,.25);border-radius:var(--radius);
       padding:14px 16px;display:flex;align-items:center;gap:12px;margin-top:20px}
.rt{font-size:12px;color:var(--danger);font-weight:600}
.rs{font-size:10px;color:var(--muted);margin-top:2px}
.br{padding:7px 14px;border-radius:8px;background:rgba(255,78,106,.1);
    color:var(--danger);font-family:'Courier New',monospace;
    font-size:11px;cursor:pointer;font-weight:600;
    border:1px solid rgba(255,78,106,.25)}
</style>
</head>
<body>
<div class="page">

  <div class="header">
    <div class="tag">Configuration</div>
    <h1>System Settings</h1>
    <p class="sub">WiFi credentials &amp; CoreIOT integration</p>
  </div>

  <div class="sbar">
    <div class="dot {{DOT_CLASS}}"></div>
    <div class="si">
      <div class="st">CoreIOT Cloud</div>
      <div class="ss">{{STATUS_TEXT}}</div>
    </div>
    <div class="chip {{CHIP_CLASS}}">{{CHIP_TEXT}}</div>
  </div>

  <div class="section">
    <div class="label"><span>WiFi Network</span></div>
    <div class="card">
      <div class="field">
        <div class="fl">SSID — Tên mạng</div>
        <input id="ssid" type="text"
               placeholder="Nhập tên WiFi..."
               value="{{SSID}}">
      </div>
      <div class="field">
        <div class="fl">Password</div>
        <input id="pass" type="password"
               placeholder="Nhập mật khẩu...">
        <button class="eye"
          onclick="let i=document.getElementById('pass');
                   i.type=i.type==='password'?'text':'password'">
          &#128065;
        </button>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="label"><span>CoreIOT</span></div>
    <div class="card">
      <div class="field">
        <div class="fl">Device Access Token</div>
        <input id="token" class="token" type="text"
               placeholder="Paste token từ app.coreiot.io..."
               value="{{TOKEN}}">
      </div>
    </div>
  </div>

  <div class="section">
    <div class="label"><span>Options</span></div>
    <div class="card">
      <div class="trow">
        <div>
          <div class="tl">Auto-reconnect</div>
          <div class="ts">Tự kết nối lại khi mất WiFi</div>
        </div>
        <div class="tog" onclick="this.classList.toggle('off')"></div>
      </div>
      <div class="trow">
        <div>
          <div class="tl">Keep AP alive</div>
          <div class="ts">Giữ hotspot khi đang STA mode</div>
        </div>
        <div class="tog" onclick="this.classList.toggle('off')"></div>
      </div>
    </div>
  </div>

  <div class="bgrp">
    <button class="btn bs" onclick="window.location='/'">&#8592; Back</button>
    <button class="btn bp" id="sbtn" onclick="saveConfig()">
      Save &amp; Connect
    </button>
  </div>

  <div id="msg"></div>

  <div class="rzone">
    <div style="font-size:18px;flex-shrink:0">&#9888;</div>
    <div style="flex:1">
      <div class="rt">Reset config</div>
      <div class="rs">Xóa toàn bộ WiFi &amp; Token đã lưu</div>
    </div>
    <button class="br" onclick="doReset()">Reset</button>
  </div>

</div>
<script>
function msg(t,ok){
  let m=document.getElementById('msg');
  m.innerText=t; m.className=ok?'ok':'err'; m.style.display='block';
}
function saveConfig(){
  let s=document.getElementById('ssid').value.trim();
  let p=document.getElementById('pass').value;
  let t=document.getElementById('token').value.trim();
  if(!s){msg('Nhập tên WiFi!',false);return}
  if(!t){msg('Nhập CoreIOT Token!',false);return}
  let b=document.getElementById('sbtn');
  b.disabled=true; b.innerText='Đang kết nối...';
  fetch('/connect?ssid='+encodeURIComponent(s)
               +'&pass='+encodeURIComponent(p)
               +'&token='+encodeURIComponent(t))
    .then(r=>r.text())
    .then(m=>{msg(m,true);b.disabled=false;b.innerText='Save & Connect'})
    .catch(()=>{msg('Lỗi!',false);b.disabled=false;b.innerText='Save & Connect'});
}
function doReset(){
  if(!confirm('Xóa toàn bộ config?'))return;
  fetch('/reset').then(()=>msg('Đã reset! Đang restart...',true));
}
</script>
</body>
</html>
)rawhtml";

extern void task_websever(void *pvParameter);
#endif