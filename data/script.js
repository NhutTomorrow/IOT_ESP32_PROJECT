/* ════════════════════════════════════════
   SMART SENSOR HUB — script.js
   ════════════════════════════════════════ */

// ── State ──
let socket = null;
let ledOn  = false;
let neoOn  = false;

// ── WebSocket ──
function initWS() {
  socket = new WebSocket('ws://' + location.hostname + '/ws');

  socket.onopen = () => {
    setWsStatus(true);
  };

  socket.onmessage = (e) => {
    try {
      const d = JSON.parse(e.data);
      updateSensors(d);
    } catch (_) {}
  };

  socket.onclose = () => {
    setWsStatus(false);
    setTimeout(initWS, 2500);
  };

  socket.onerror = () => {
    socket.close();
  };
}

function send(msg) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(msg);
  }
}

function setWsStatus(online) {
  const dot   = document.getElementById('ws-dot');
  const label = document.getElementById('ws-label');
  const ind   = document.getElementById('ws-indicator');
  if (online) {
    dot.classList.add('on');
    ind.classList.add('on');
    label.textContent = 'ONLINE';
  } else {
    dot.classList.remove('on');
    ind.classList.remove('on');
    label.textContent = 'OFFLINE';
  }
}

// ── Sensor update ──
function updateSensors(d) {
  if (d.temp !== undefined) {
    document.getElementById('temp').textContent = parseFloat(d.temp).toFixed(1);
    // Bar: map 10–50°C → 0–100%
    const pct = Math.min(100, Math.max(0, (d.temp - 10) / 40 * 100));
    document.getElementById('temp-fill').style.width = pct + '%';
  }
  if (d.hum !== undefined) {
    document.getElementById('hum').textContent = parseFloat(d.hum).toFixed(1);
    document.getElementById('hum-fill').style.width = Math.min(100, d.hum) + '%';
  }
  if (d.ml !== undefined) {
    const el  = document.getElementById('ml');
    const pip = document.getElementById('ml-pip');
    el.textContent = d.ml;
    pip.className  = 'status-pip';
    if (d.ml === 'Normal')         pip.classList.add('ok');
    else if (d.ml.includes('Warn')) pip.classList.add('warn');
    else                            pip.classList.add('crit');
  }
  if (d.wifi !== undefined) {
    const el  = document.getElementById('wifi-text');
    const pip = document.getElementById('wifi-pip');
    el.textContent = d.wifi ? 'Online' : 'Offline';
    pip.className  = 'status-pip' + (d.wifi ? ' ok' : '');
  }
}

// ── LED toggle ──
function toggleLed() {
  ledOn = !ledOn;
  const card = document.getElementById('led-card');
  const state = document.getElementById('led-state');

  if (ledOn) {
    card.classList.add('active');
    state.textContent = 'ON';
    send('led:on');
  } else {
    card.classList.remove('active');
    state.textContent = 'OFF';
    send('led:off');
  }
}

// ── NeoPixel ──
function setNeo(r, g, b) {
  neoOn = (r + g + b) > 0;
  const card  = document.getElementById('neo-card-toggle');
  const state = document.getElementById('neo-state');
  const dot   = document.getElementById('neo-dot');
  const ring  = document.getElementById('neo-ring-indicator');

  if (neoOn) {
    card.classList.add('active');
    state.textContent = 'ON';
    // tint the dot with the chosen color
    const hex = `rgb(${r},${g},${b})`;
    dot.style.background  = hex;
    dot.style.boxShadow   = `0 0 12px ${hex}, 0 0 24px rgba(${r},${g},${b},0.4)`;
    ring.style.borderColor = hex;
    ring.style.boxShadow  = `0 0 16px rgba(${r},${g},${b},0.4)`;
  } else {
    card.classList.remove('active');
    state.textContent = 'OFF';
    dot.style.background  = '';
    dot.style.boxShadow   = '';
    ring.style.borderColor = '';
    ring.style.boxShadow  = '';
  }

  send(`neo:${r},${g},${b}`);
}

function previewColor(hex) {
  // live preview on color picker border
  document.getElementById('colorpicker').style.outline = `2px solid ${hex}`;
}

function applyCustomColor() {
  const hex = document.getElementById('colorpicker').value;
  const r = parseInt(hex.slice(1,3), 16);
  const g = parseInt(hex.slice(3,5), 16);
  const b = parseInt(hex.slice(5,7), 16);
  setNeo(r, g, b);
}

// ════════════════════════════════════════
//  STARFIELD CANVAS
// ════════════════════════════════════════
(function initStars() {
  const canvas = document.getElementById('stars');
  const ctx    = canvas.getContext('2d');
  let stars    = [];
  let W, H;

  function resize() {
    W = canvas.width  = window.innerWidth;
    H = canvas.height = window.innerHeight;
  }

  function createStars(n) {
    stars = [];
    for (let i = 0; i < n; i++) {
      stars.push({
        x:     Math.random() * W,
        y:     Math.random() * H,
        r:     Math.random() * 1.2 + 0.2,
        alpha: Math.random() * 0.6 + 0.1,
        speed: Math.random() * 0.3 + 0.05,
        phase: Math.random() * Math.PI * 2,
      });
    }
  }

  let t = 0;
  function draw() {
    ctx.clearRect(0, 0, W, H);
    t += 0.008;
    stars.forEach(s => {
      const twinkle = s.alpha + Math.sin(t * s.speed + s.phase) * 0.15;
      ctx.beginPath();
      ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(180,220,255,${Math.max(0, twinkle)})`;
      ctx.fill();
    });
    requestAnimationFrame(draw);
  }

  window.addEventListener('resize', () => { resize(); createStars(120); });
  resize();
  createStars(120);
  draw();
})();

// ════════════════════════════════════════
//  INIT
// ════════════════════════════════════════
document.addEventListener('DOMContentLoaded', () => {
  initWS();
});
