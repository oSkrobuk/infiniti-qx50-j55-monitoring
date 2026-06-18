#include "WebManager.h"

#include "ConfigManager.h"
#include <Update.h>
#include <WiFi.h>

// HTML страница хранится во флеш-памяти (PROGMEM), не занимает RAM
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>INFINITI QX50 J55 MONITORING</title>
<style>
  :root {
    --bg: #0d0d0d;
    --card: #1a1a1a;
    --border: #2a2a2a;
    --accent: #c9a84c;
    --text: #e0e0e0;
    --muted: #888;
    --green: #4caf50;
    --red: #f44336;
    --blue: #2196f3;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', Arial, sans-serif;
    min-height: 100vh;
    padding: 20px;
  }
  header {
    text-align: center;
    padding: 24px 0 8px;
    border-bottom: 1px solid var(--border);
    margin-bottom: 20px;
  }
  header h1 {
    font-size: 1.4rem;
    letter-spacing: 3px;
    color: var(--accent);
    text-transform: uppercase;
  }
  header h2 {
    font-size: 0.85rem;
    letter-spacing: 4px;
    color: var(--muted);
    text-transform: uppercase;
    font-weight: 400;
    margin-top: 4px;
  }
  header p {
    color: var(--muted);
    font-size: 0.8rem;
    margin-top: 6px;
    letter-spacing: 2px;
    text-transform: uppercase;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 16px;
    max-width: 960px;
    margin: 0 auto 20px;
  }
  .card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 16px;
  }
  .card-title {
    font-size: 0.85rem;
    letter-spacing: 2px;
    text-transform: uppercase;
    color: var(--accent);
    margin-bottom: 14px;
  }
  /* Два поля в одну строку */
  .row2 {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }
  /* Три поля в одну строку */
  .row3 {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 8px;
  }
  /* Четыре поля в одну строку */
  .row4 {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr 1fr;
    gap: 8px;
  }
  .field label {
    display: block;
    font-size: 0.7rem;
    color: var(--muted);
    margin-bottom: 4px;
    letter-spacing: 0.5px;
    white-space: nowrap;
  }
  .field input[type="number"] {
    width: 100%;
    background: #111;
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--text);
    font-size: 0.95rem;
    padding: 7px 8px;
    outline: none;
    transition: border-color 0.2s;
    text-align: center;
  }
  .field input.f-min:focus    { border-color: var(--blue); }
  .field input.f-target:focus { border-color: var(--green); }
  .field input.f-max:focus    { border-color: var(--red); }
  .legend {
    display: flex;
    gap: 14px;
    font-size: 0.75rem;
    color: var(--muted);
    margin-bottom: 14px;
    flex-wrap: wrap;
    max-width: 960px;
    margin-left: auto;
    margin-right: auto;
  }
  .legend span { display: flex; align-items: center; gap: 4px; }
  .dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
  .actions {
    max-width: 960px;
    margin: 0 auto;
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
  }
  button {
    flex: 1;
    min-width: 140px;
    padding: 12px 20px;
    border: none;
    border-radius: 8px;
    font-size: 0.95rem;
    font-weight: 600;
    letter-spacing: 1px;
    cursor: pointer;
    transition: opacity 0.2s, transform 0.1s;
  }
  button:active { transform: scale(0.97); }
  button:disabled { opacity: 0.4; cursor: not-allowed; }
  .btn-save    { background: var(--accent); color: #000; }
  .btn-default { background: var(--border); color: var(--muted); border: 1px solid #444; }
  /* OTA styles */
  .ota-wrap {
    max-width: 960px;
    margin: 20px auto 0;
  }
  .ota-row {
    display: flex;
    gap: 12px;
    align-items: center;
    flex-wrap: wrap;
  }
  .btn-ota-label {
    display: inline-block;
    padding: 10px 18px;
    background: var(--border);
    border: 1px solid #444;
    border-radius: 8px;
    font-size: 0.9rem;
    font-weight: 600;
    letter-spacing: 1px;
    cursor: pointer;
    color: var(--muted);
    transition: opacity 0.2s;
    white-space: nowrap;
  }
  .btn-ota-label:hover { opacity: 0.8; }
  .ota-filename {
    flex: 1;
    font-size: 0.85rem;
    color: var(--muted);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .btn-ota-upload {
    background: #1565c0;
    color: #fff;
    min-width: 140px;
    flex: 0 0 auto;
  }
  .ota-progress { display: none; margin-top: 14px; }
  .ota-bar-bg {
    background: #111;
    border-radius: 4px;
    height: 8px;
    overflow: hidden;
    border: 1px solid var(--border);
  }
  .ota-bar-fill {
    height: 100%;
    width: 0%;
    background: var(--accent);
    border-radius: 4px;
    transition: width 0.25s;
  }
  .ota-status {
    font-size: 0.8rem;
    color: var(--muted);
    margin-top: 7px;
    text-align: center;
    letter-spacing: 0.5px;
  }
  .toast {
    position: fixed;
    bottom: 24px;
    left: 50%;
    transform: translateX(-50%) translateY(120px);
    background: #222;
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 12px 24px;
    font-size: 0.9rem;
    transition: transform 0.4s ease, visibility 0s linear 0.4s;
    z-index: 100;
    white-space: normal;
    max-width: calc(100vw - 48px);
    text-align: center;
    visibility: hidden;
  }
  .toast.show {
    transform: translateX(-50%) translateY(0);
    visibility: visible;
    transition: transform 0.4s ease, visibility 0s linear 0s;
  }
  .toast.ok   { border-color: var(--green); color: var(--green); }
  .toast.err  { border-color: var(--red);   color: var(--red); }
  footer {
    text-align: center;
    color: var(--muted);
    font-size: 0.75rem;
    margin-top: 36px;
    letter-spacing: 1px;
  }
</style>
</head>
<body>

<header>
  <h1>&#9670; INFINITI QX50 J55 &#9670;</h1>
  <h2>MONITORING - Config Editor</h2>
</header>

<div class="legend" style="margin-bottom:14px;">
  <span><span class="dot" style="background:#2196f3"></span> Минимум</span>
  <span><span class="dot" style="background:#4caf50"></span> Целевая</span>
  <span><span class="dot" style="background:#f44336"></span> Максимум</span>
</div>

<form id="configForm">
<div class="grid">

  <div class="card">
    <div class="card-title">&#128167; E-OIL — Моторное масло</div>
    <div class="row3">
      <div class="field">
        <label>Мин, °C</label>
        <input class="f-min" type="number" step="0.5" name="oil_min" required>
      </div>
      <div class="field">
        <label>Цель, °C</label>
        <input class="f-target" type="number" step="0.5" name="oil_target" required>
      </div>
      <div class="field">
        <label>Макс, °C</label>
        <input class="f-max" type="number" step="0.5" name="oil_max" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#10052; E-COOL — Антифриз ДВС</div>
    <div class="row3">
      <div class="field">
        <label>Мин, °C</label>
        <input class="f-min" type="number" step="0.5" name="coolant_min" required>
      </div>
      <div class="field">
        <label>Цель, °C</label>
        <input class="f-target" type="number" step="0.5" name="coolant_target" required>
      </div>
      <div class="field">
        <label>Макс, °C</label>
        <input class="f-max" type="number" step="0.5" name="coolant_max" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#127777; R-COOL — Антифриз радиатора</div>
    <div class="row3">
      <div class="field">
        <label>Мин, °C</label>
        <input class="f-min" type="number" step="0.5" name="radiator_min" required>
      </div>
      <div class="field">
        <label>Цель, °C</label>
        <input class="f-target" type="number" step="0.5" name="radiator_target" required>
      </div>
      <div class="field">
        <label>Макс, °C</label>
        <input class="f-max" type="number" step="0.5" name="radiator_max" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#9881; T-OIL — Масло АКПП</div>
    <div class="row3">
      <div class="field">
        <label>Мин, °C</label>
        <input class="f-min" type="number" step="0.5" name="transmission_min" required>
      </div>
      <div class="field">
        <label>Цель, °C</label>
        <input class="f-target" type="number" step="0.5" name="transmission_target" required>
      </div>
      <div class="field">
        <label>Макс, °C</label>
        <input class="f-max" type="number" step="0.5" name="transmission_max" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#9889; RPM — Обороты двигателя</div>
    <div class="row3">
      <div class="field">
        <label>Нач. зелёной</label>
        <input class="f-min" type="number" step="50" name="rpm_green_start" required>
      </div>
      <div class="field">
        <label>Кон. зелёной</label>
        <input class="f-target" type="number" step="50" name="rpm_green_end" required>
      </div>
      <div class="field">
        <label>Нач. красной</label>
        <input class="f-max" type="number" step="50" name="rpm_red_start" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#128167; EOP — Давление масла</div>
    <div class="row3">
      <div class="field">
        <label>Порог RPM</label>
        <input class="f-target" type="number" step="100" name="oil_pressure_rpm_threshold" required>
      </div>
      <div class="field">
        <label>Мин &lt;порога, бар</label>
        <input class="f-min" type="number" step="0.1" name="oil_pressure_min_low" required>
      </div>
      <div class="field">
        <label>Мин &ge;порога, бар</label>
        <input class="f-max" type="number" step="0.1" name="oil_pressure_min_high" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#128168; BOOST — Давление наддува</div>
    <div class="row2">
      <div class="field">
        <label>Синий до, В</label>
        <input class="f-min" type="number" step="0.05" name="boost_blue_max" required>
      </div>
      <div class="field">
        <label>Зелёный от, В</label>
        <input class="f-target" type="number" step="0.05" name="boost_green_min" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#128267; BATTERY — Бортовая сеть</div>
    <div class="row4">
      <div class="field">
        <label>Красный &lt;, В</label>
        <input class="f-min" type="number" step="0.1" name="battery_red_low" required>
      </div>
      <div class="field">
        <label>Зелёный от, В</label>
        <input class="f-target" type="number" step="0.1" name="battery_green_min" required>
      </div>
      <div class="field">
        <label>Зелёный до, В</label>
        <input class="f-target" type="number" step="0.1" name="battery_green_max" required>
      </div>
      <div class="field">
        <label>Красный &gt;, В</label>
        <input class="f-max" type="number" step="0.1" name="battery_red_high" required>
      </div>
    </div>
  </div>

</div>

<div class="actions">
  <button type="button" class="btn-default" id="btnDefault">&#8635; По умолчанию</button>
  <button type="submit" class="btn-save"    id="btnSave">&#10003; Сохранить</button>
</div>
</form>

<!-- OTA Firmware Update -->
<div class="ota-wrap">
  <div class="card">
    <div class="card-title">&#8593; OTA — Обновление прошивки</div>
    <div class="ota-row">
      <label class="btn-ota-label" for="otaFile">&#128190; Выбрать .bin</label>
      <input type="file" id="otaFile" accept=".bin" style="display:none">
      <span class="ota-filename" id="otaFileName">файл не выбран</span>
      <button type="button" class="btn-ota-upload" id="btnOta" disabled>&#8593; Загрузить</button>
    </div>
    <div class="ota-progress" id="otaProgress">
      <div class="ota-bar-bg">
        <div class="ota-bar-fill" id="otaBar"></div>
      </div>
      <div class="ota-status" id="otaStatus"></div>
    </div>
  </div>
</div>

<div class="toast" id="toast"></div>

<footer>Infiniti QX50 J55 Monitoring &mdash; ESP32</footer>

<script>
let original = {};

function showToast(msg, type) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast ' + type + ' show';
  setTimeout(() => { t.className = 'toast'; }, 5000);
}

function fillForm(cfg) {
  ['oil','coolant','radiator','transmission'].forEach(f => {
    ['min','target','max'].forEach(s => {
      const el = document.querySelector(`[name="${f}_${s}"]`);
      if (el) el.value = cfg[f][s];
    });
  });
  // RPM fields
  if (cfg.rpm) {
    const gs = document.querySelector('[name="rpm_green_start"]');
    const ge = document.querySelector('[name="rpm_green_end"]');
    const rs = document.querySelector('[name="rpm_red_start"]');
    if (gs) gs.value = cfg.rpm.green_start;
    if (ge) ge.value = cfg.rpm.green_end;
    if (rs) rs.value = cfg.rpm.red_start;
  }
  // Oil pressure
  if (cfg.oil_pressure) {
    const rt  = document.querySelector('[name="oil_pressure_rpm_threshold"]');
    const ml  = document.querySelector('[name="oil_pressure_min_low"]');
    const mh  = document.querySelector('[name="oil_pressure_min_high"]');
    if (rt)  rt.value = cfg.oil_pressure.rpm_threshold;
    if (ml)  ml.value = cfg.oil_pressure.min_low;
    if (mh)  mh.value = cfg.oil_pressure.min_high;
  }
  // Boost
  if (cfg.boost) {
    const bm = document.querySelector('[name="boost_blue_max"]');
    const gm = document.querySelector('[name="boost_green_min"]');
    if (bm) bm.value = cfg.boost.blue_max;
    if (gm) gm.value = cfg.boost.green_min;
  }
  // Battery
  if (cfg.battery) {
    const rl = document.querySelector('[name="battery_red_low"]');
    const gn = document.querySelector('[name="battery_green_min"]');
    const gx = document.querySelector('[name="battery_green_max"]');
    const rh = document.querySelector('[name="battery_red_high"]');
    if (rl) rl.value = cfg.battery.red_low;
    if (gn) gn.value = cfg.battery.green_min;
    if (gx) gx.value = cfg.battery.green_max;
    if (rh) rh.value = cfg.battery.red_high;
  }
}

function readForm() {
  const d = {};
  ['oil','coolant','radiator','transmission'].forEach(s => {
    d[s] = {
      min:    parseFloat(document.querySelector(`[name="${s}_min"]`).value),
      target: parseFloat(document.querySelector(`[name="${s}_target"]`).value),
      max:    parseFloat(document.querySelector(`[name="${s}_max"]`).value),
    };
  });
  d.rpm = {
    green_start: parseFloat(document.querySelector('[name="rpm_green_start"]').value),
    green_end:   parseFloat(document.querySelector('[name="rpm_green_end"]').value),
    red_start:   parseFloat(document.querySelector('[name="rpm_red_start"]').value),
  };
  d.oil_pressure = {
    rpm_threshold: parseFloat(document.querySelector('[name="oil_pressure_rpm_threshold"]').value),
    min_low:       parseFloat(document.querySelector('[name="oil_pressure_min_low"]').value),
    min_high:      parseFloat(document.querySelector('[name="oil_pressure_min_high"]').value),
  };
  d.boost = {
    blue_max:  parseFloat(document.querySelector('[name="boost_blue_max"]').value),
    green_min: parseFloat(document.querySelector('[name="boost_green_min"]').value),
  };
  d.battery = {
    red_low:   parseFloat(document.querySelector('[name="battery_red_low"]').value),
    green_min: parseFloat(document.querySelector('[name="battery_green_min"]').value),
    green_max: parseFloat(document.querySelector('[name="battery_green_max"]').value),
    red_high:  parseFloat(document.querySelector('[name="battery_red_high"]').value),
  };
  return d;
}

function validateForm(data) {
  const sensors = ['oil','coolant','radiator','transmission'];
  for (const s of sensors) {
    if (data[s].min >= data[s].target) return `${s}: минимум должен быть меньше целевой`;
    if (data[s].target >= data[s].max) return `${s}: целевая должна быть меньше максимума`;
  }
  if (data.rpm.green_start >= data.rpm.green_end)
    return 'RPM: начало зелёной зоны должно быть меньше конца';
  if (data.rpm.green_end >= data.rpm.red_start)
    return 'RPM: конец зелёной зоны должен быть меньше начала красной';
  if (data.oil_pressure.min_low <= 0 || data.oil_pressure.min_high <= 0)
    return 'EOP: минимальное давление должно быть больше 0';
  if (data.oil_pressure.min_low >= data.oil_pressure.min_high)
    return 'EOP: мин при низких оборотах должен быть меньше мин при высоких';
  if (data.boost.blue_max >= data.boost.green_min)
    return 'BOOST: граница синего должна быть меньше границы зелёного';
  if (data.battery.red_low >= data.battery.green_min)
    return 'BATTERY: нижний красный порог должен быть меньше начала зелёной зоны';
  if (data.battery.green_min >= data.battery.green_max)
    return 'BATTERY: начало зелёной зоны должно быть меньше конца';
  if (data.battery.green_max >= data.battery.red_high)
    return 'BATTERY: конец зелёной зоны должен быть меньше верхнего красного порога';
  return null;
}

async function loadConfig() {
  try {
    const r = await fetch('/config');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const cfg = await r.json();
    original = JSON.parse(JSON.stringify(cfg));
    fillForm(cfg);
  } catch(e) {
    showToast('Ошибка загрузки: ' + e.message, 'err');
  }
}

document.getElementById('configForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const data = readForm();
  const err = validateForm(data);
  if (err) { showToast('⚠ ' + err, 'err'); return; }

  const btn = document.getElementById('btnSave');
  btn.disabled = true;
  try {
    const r = await fetch('/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    });
    const text = await r.text();
    if (!r.ok) throw new Error('HTTP ' + r.status + ': ' + text);
    original = JSON.parse(JSON.stringify(data));
    showToast('✓ Конфиг сохранён!', 'ok');
  } catch(e) {
    showToast('Ошибка: ' + e.message, 'err');
  } finally {
    btn.disabled = false;
  }
});

document.getElementById('btnDefault').addEventListener('click', async () => {
  if (!confirm('Сбросить все пороги к значениям по умолчанию?')) return;
  const btn = document.getElementById('btnDefault');
  btn.disabled = true;
  try {
    const r = await fetch('/reset', { method: 'POST' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const cfg = await r.json();
    original = JSON.parse(JSON.stringify(cfg));
    fillForm(cfg);
    showToast('Значения по умолчанию восстановлены.', 'ok');
  } catch(e) {
    showToast('Ошибка: ' + e.message, 'err');
  } finally {
    btn.disabled = false;
  }
});

// ── OTA ──────────────────────────────────────────────────
const otaFile     = document.getElementById('otaFile');
const otaFileName = document.getElementById('otaFileName');
const btnOta      = document.getElementById('btnOta');
const otaProgress = document.getElementById('otaProgress');
const otaBar      = document.getElementById('otaBar');
const otaStatus   = document.getElementById('otaStatus');

otaFile.addEventListener('change', () => {
  if (otaFile.files.length) {
    const f = otaFile.files[0];
    otaFileName.textContent = f.name + '  (' + Math.round(f.size / 1024) + ' KB)';
    btnOta.disabled = false;
  } else {
    otaFileName.textContent = 'файл не выбран';
    btnOta.disabled = true;
  }
});

btnOta.addEventListener('click', () => {
  if (!otaFile.files.length) return;
  if (!confirm('Загрузить новую прошивку?\nУстройство перезагрузится после прошивки.')) return;

  const formData = new FormData();
  formData.append('firmware', otaFile.files[0], otaFile.files[0].name);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');

  btnOta.disabled = true;
  otaBar.style.background = 'var(--accent)';
  otaBar.style.width = '0%';
  otaStatus.textContent = 'Подготовка...';
  otaProgress.style.display = 'block';

  xhr.upload.addEventListener('progress', (e) => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded / e.total * 100);
      otaBar.style.width = pct + '%';
      otaStatus.textContent =
        'Загрузка: ' + pct + '%  (' +
        Math.round(e.loaded / 1024) + ' / ' +
        Math.round(e.total  / 1024) + ' KB)';
    }
  });

  xhr.addEventListener('load', () => {
    if (xhr.status === 200) {
      otaBar.style.width = '100%';
      otaBar.style.background = 'var(--green)';
      otaStatus.textContent = '✓ Прошивка записана. Перезагрузка...';
    } else {
      otaBar.style.background = 'var(--red)';
      otaStatus.textContent = '✗ Ошибка: ' + xhr.responseText;
      btnOta.disabled = false;
    }
  });

  xhr.addEventListener('error', () => {
    otaBar.style.background = 'var(--red)';
    otaStatus.textContent = '✗ Сетевая ошибка';
    btnOta.disabled = false;
  });

  xhr.send(formData);
});

loadConfig();
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────

WebManager::WebManager(const char *ssid, const char *password)
    : ssid_(ssid), password_(password), server_(80)
{
}

void WebManager::begin()
{
    WiFi.softAP(ssid_, password_);
    Serial.printf("[WiFi] AP запущен  SSID: %s\r\n", ssid_);

    server_.on("/",            HTTP_GET,  [this]() { handle_root(); });
    server_.on("/config",      HTTP_GET,  [this]() { handle_get_config(); });
    server_.on("/config",      HTTP_POST, [this]() { handle_post_config(); });
    server_.on("/reset",       HTTP_POST, [this]() { handle_reset(); });
    server_.on("/favicon.ico", HTTP_GET,  [this]() { server_.send(204); });

    // GET /update — та же страница, что и корень
    server_.on("/update", HTTP_GET, [this]() { handle_update_page(); });

    // POST /update — загрузка .bin (два обработчика: завершение + чанки данных)
    server_.on("/update", HTTP_POST,
        [this]() { // вызывается ПОСЛЕ завершения загрузки файла
            bool ok = !Update.hasError();
            server_.sendHeader("Connection", "close");
            if (ok) {
                server_.send(200, "application/json", "{\"ok\":true}");
                Serial.println("[OTA] Успех — перезагрузка...");
                delay(300);
                ESP.restart();
            } else {
                String err = Update.errorString();
                server_.send(500, "application/json",
                    "{\"error\":\"" + err + "\"}");
                Serial.printf("[OTA] Ошибка: %s\r\n", err.c_str());
            }
        },
        [this]() { handle_update_upload(); } // вызывается для каждого чанка
    );

    server_.onNotFound([this]() { handle_not_found(); });

    const char *headers[] = {"Content-Length", "Content-Type"};
    server_.collectHeaders(headers, 2);

    server_.begin();
    Serial.println("[Web] HTTP сервер запущен на порту 80");
}

void WebManager::handle()
{
    server_.handleClient();
}

String WebManager::get_ip() const
{
    return WiFi.softAPIP().toString();
}

void WebManager::handle_root()
{
    server_.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebManager::handle_get_config()
{
    server_.send(200, "application/json", config.to_json());
}

void WebManager::handle_post_config()
{
    String body;

    if (server_.hasArg("plain")) {
        body = server_.arg("plain");
    }

    if (body.isEmpty()) {
        WiFiClient client = server_.client();
        int len = server_.header("Content-Length").toInt();
        if (len > 0 && len < 2048) {
            body.reserve(len);
            unsigned long deadline = millis() + 300;
            while (static_cast<int>(body.length()) < len && millis() < deadline) {
                if (client.available()) body += static_cast<char>(client.read());
                else delay(1);
            }
        }
    }

    body.trim();
    Serial.printf("[Web] POST /config  %u байт\r\n", body.length());

    if (body.isEmpty()) {
        server_.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    if (config.from_json(body)) {
        server_.send(200, "application/json", "{\"ok\":true}");
    } else {
        server_.send(400, "application/json", "{\"error\":\"invalid json\"}");
    }
}

void WebManager::handle_reset()
{
    Serial.println("[Web] POST /reset — сброс к значениям по умолчанию");
    if (config.reset_to_defaults()) {
        server_.send(200, "application/json", config.to_json());
    } else {
        server_.send(500, "application/json", "{\"error\":\"reset failed\"}");
    }
}

void WebManager::handle_not_found()
{
    Serial.printf("[Web] 404 %s %s\r\n",
        server_.method() == HTTP_GET ? "GET" : "POST",
        server_.uri().c_str());
    server_.send(404, "text/plain", "Not found");
}

// ── OTA ──────────────────────────────────────────────────────────────────────

void WebManager::handle_update_page()
{
    // OTA форма встроена в главную страницу — просто редиректим
    server_.sendHeader("Location", "/");
    server_.send(302);
}

void WebManager::handle_update_upload()
{
    HTTPUpload &upload = server_.upload();

    switch (upload.status) {
    case UPLOAD_FILE_START:
        Serial.printf("[OTA] Начало: %s\r\n", upload.filename.c_str());
        // UPDATE_SIZE_UNKNOWN — Update сам определит конец по закрытию соединения
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.printf("[OTA] begin() ошибка: %s\r\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_WRITE:
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Serial.printf("[OTA] write() ошибка: %s\r\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_END:
        if (Update.end(true)) { // true = финализировать (проверить MD5/размер)
            Serial.printf("[OTA] Завершено: %u байт\r\n", upload.totalSize);
        } else {
            Serial.printf("[OTA] end() ошибка: %s\r\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_ABORTED:
        Update.abort();
        Serial.println("[OTA] Загрузка прервана");
        break;

    default:
        break;
    }
}
