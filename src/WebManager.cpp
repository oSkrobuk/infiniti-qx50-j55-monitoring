#include "WebManager.h"

#include "ConfigManager.h"
#include "AlertManager.h"
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
  .section-title {
    max-width: 960px;
    margin: 28px auto 12px;
    font-size: 0.8rem;
    letter-spacing: 3px;
    text-transform: uppercase;
    color: var(--accent);
    border-bottom: 1px solid var(--border);
    padding-bottom: 8px;
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
  .btn-danger  { background: #3a1010; color: var(--red); border: 1px solid #6a1010; }
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
  /* ── Toggle switch ────────────────────────────────── */
  .toggle-wrap {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 12px;
  }
  .toggle {
    position: relative;
    display: inline-block;
    width: 44px;
    height: 24px;
    flex-shrink: 0;
  }
  .toggle input { opacity: 0; width: 0; height: 0; }
  .slider {
    position: absolute;
    cursor: pointer;
    top: 0; left: 0; right: 0; bottom: 0;
    background: #333;
    border-radius: 24px;
    transition: .3s;
  }
  .slider:before {
    position: absolute;
    content: "";
    height: 18px;
    width: 18px;
    left: 3px;
    bottom: 3px;
    background: #777;
    border-radius: 50%;
    transition: .3s;
  }
  input:checked + .slider { background: var(--green); }
  input:checked + .slider:before { transform: translateX(20px); background: #fff; }
  .toggle-label {
    font-size: 0.8rem;
    color: var(--muted);
    letter-spacing: 0.5px;
  }
  /* ── Alert history table ──────────────────────────── */
  .alert-actions {
    display: flex;
    justify-content: flex-end;
    margin-bottom: 10px;
  }
  .btn-clear-alerts {
    flex: 0 0 auto;
    min-width: 0;
    padding: 8px 16px;
    font-size: 0.8rem;
  }
  .alert-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.85rem;
  }
  .alert-table th {
    color: var(--accent);
    font-weight: 600;
    padding: 6px 8px;
    border-bottom: 1px solid var(--border);
    text-align: left;
    font-size: 0.75rem;
    letter-spacing: 1px;
    text-transform: uppercase;
  }
  .alert-table td {
    padding: 7px 8px;
    border-bottom: 1px solid #1c1c1c;
    vertical-align: top;
  }
  .alert-code {
    color: var(--red);
    font-weight: 700;
    font-family: monospace;
    font-size: 0.95rem;
    white-space: nowrap;
  }
  .alert-desc { color: var(--text); }
  .alert-count {
    color: var(--accent);
    font-weight: 700;
    text-align: right;
    white-space: nowrap;
  }
  .alert-empty {
    color: var(--muted);
    font-style: italic;
    text-align: center;
    padding: 16px 0;
  }
  /* ── Checks params row ────────────────────────────── */
  .check-params {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
  }
  .check-params .field {
    flex: 1 1 80px;
    min-width: 70px;
  }
</style>
</head>
<body>

<header>
  <h1>&#9670; INFINITI QX50 J55 &#9670;</h1>
  <h2>MONITORING - Редактор конфигурации</h2>
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

  <div class="card">
    <div class="card-title">&#9201; RPM-POLL — Время опроса RPM</div>
    <div class="row2">
      <div class="field">
        <label>Зелёный до, с</label>
        <input class="f-target" type="number" step="0.01" name="poll_time_green_max" required>
      </div>
      <div class="field">
        <label>Красный от, с</label>
        <input class="f-max" type="number" step="0.01" name="poll_time_red_min" required>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#9881; SYSTEM — Параметры CAN-опроса</div>
    <div class="row2">
      <div class="field">
        <label>Интервал опроса, мс</label>
        <input class="f-target" type="number" step="1" min="10" name="system_poll_interval_ms" required>
      </div>
      <div class="field">
        <label>Устаревание CAN, мс</label>
        <input class="f-target" type="number" step="100" min="100" name="system_stale_ms" required>
      </div>
    </div>
  </div>

</div>

<div class="actions">
  <button type="button" class="btn-default" id="btnDefault">&#8635; По умолчанию</button>
  <button type="submit" class="btn-save"    id="btnSave">&#10003; Сохранить</button>
</div>
</form>

<!-- ── Alert Checks Configuration ───────────────────────────────────────── -->
<div class="section-title">&#9888; CHECKS — Конфигурация проверок</div>
<div class="grid" id="checksGrid">
  <!-- Заполняется JavaScript -->
</div>
<div class="actions" style="margin-bottom:8px;">
  <button type="button" class="btn-save" id="btnSaveChecks">&#10003; Сохранить проверки</button>
</div>

<!-- ── Alert History ─────────────────────────────────────────────────────── -->
<div class="section-title">&#128680; ALERTS — История срабатываний</div>
<div class="ota-wrap" style="margin-top:0;">
  <div class="card">
    <div class="alert-actions">
      <button type="button" class="btn-danger btn-clear-alerts" id="btnClearAlerts">
        &#128465; Очистить историю
      </button>
    </div>
    <table class="alert-table">
      <thead>
        <tr>
          <th>Код</th>
          <th>Описание</th>
          <th style="text-align:right;">Кол-во</th>
        </tr>
      </thead>
      <tbody id="alertTableBody">
        <tr><td colspan="3" class="alert-empty">Загрузка...</td></tr>
      </tbody>
    </table>
  </div>
</div>

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

// Описания всех проверок — должны совпадать с CHECK_DEFS в AlertManager.cpp
const CHECK_DEFS = [
  { code:'E01', name:'Engine Oil Temp High',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E02', name:'Engine Coolant Temp High',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E03', name:'Radiator Coolant Temp High',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E04', name:'CVT Oil Temp High',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E05', name:'Engine RPM Overspeed',
    params:[{label:'Макс. обороты', key:'param1', step:50}] },
  { code:'E06', name:'Battery Voltage Low',
    params:[{label:'Мин. напряжение, В', key:'param1', step:0.1}] },
  { code:'E07', name:'Battery Voltage High',
    params:[{label:'Макс. напряжение, В', key:'param1', step:0.1}] },
  { code:'E08', name:'Oil Pressure Low',
    params:[
      {label:'Порог оборотов', key:'param1', step:100},
      {label:'Мин. В (низк. обор.)', key:'param2', step:0.05},
      {label:'Мин. В (выс. обор.)', key:'param3', step:0.05}
    ]
  },
];

function showToast(msg, type) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast ' + type + ' show';
  setTimeout(() => { t.className = 'toast'; }, 5000);
}

// ── Config form ────────────────────────────────────────────────────────────

function fillForm(cfg) {
  ['oil','coolant','radiator','transmission'].forEach(f => {
    ['min','target','max'].forEach(s => {
      const el = document.querySelector(`[name="${f}_${s}"]`);
      if (el) el.value = cfg[f][s];
    });
  });
  if (cfg.rpm) {
    const gs = document.querySelector('[name="rpm_green_start"]');
    const ge = document.querySelector('[name="rpm_green_end"]');
    const rs = document.querySelector('[name="rpm_red_start"]');
    if (gs) gs.value = cfg.rpm.green_start;
    if (ge) ge.value = cfg.rpm.green_end;
    if (rs) rs.value = cfg.rpm.red_start;
  }
  if (cfg.oil_pressure) {
    const rt = document.querySelector('[name="oil_pressure_rpm_threshold"]');
    const ml = document.querySelector('[name="oil_pressure_min_low"]');
    const mh = document.querySelector('[name="oil_pressure_min_high"]');
    if (rt) rt.value = cfg.oil_pressure.rpm_threshold;
    if (ml) ml.value = cfg.oil_pressure.min_low;
    if (mh) mh.value = cfg.oil_pressure.min_high;
  }
  if (cfg.boost) {
    const bm = document.querySelector('[name="boost_blue_max"]');
    const gm = document.querySelector('[name="boost_green_min"]');
    if (bm) bm.value = cfg.boost.blue_max;
    if (gm) gm.value = cfg.boost.green_min;
  }
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
  if (cfg.poll_time) {
    const gm = document.querySelector('[name="poll_time_green_max"]');
    const rm = document.querySelector('[name="poll_time_red_min"]');
    if (gm) gm.value = cfg.poll_time.green_max;
    if (rm) rm.value = cfg.poll_time.red_min;
  }
  if (cfg.system) {
    const pi = document.querySelector('[name="system_poll_interval_ms"]');
    const sm = document.querySelector('[name="system_stale_ms"]');
    if (pi) pi.value = cfg.system.poll_interval_ms;
    if (sm) sm.value = cfg.system.stale_ms;
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
  d.poll_time = {
    green_max: parseFloat(document.querySelector('[name="poll_time_green_max"]').value),
    red_min:   parseFloat(document.querySelector('[name="poll_time_red_min"]').value),
  };
  d.system = {
    poll_interval_ms: parseFloat(document.querySelector('[name="system_poll_interval_ms"]').value),
    stale_ms:         parseFloat(document.querySelector('[name="system_stale_ms"]').value),
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
  if (data.poll_time.green_max <= 0 || data.poll_time.red_min <= 0)
    return 'RPM-POLL: пороги должны быть больше 0';
  if (data.poll_time.green_max >= data.poll_time.red_min)
    return 'RPM-POLL: зелёный порог должен быть меньше красного';
  if (data.system.poll_interval_ms < 10)
    return 'SYSTEM: интервал опроса не может быть меньше 10 мс';
  if (data.system.stale_ms < 100)
    return 'SYSTEM: порог устаревания не может быть меньше 100 мс';
  if (data.system.poll_interval_ms >= data.system.stale_ms)
    return 'SYSTEM: интервал опроса должен быть меньше порога устаревания';
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

// ── Checks ─────────────────────────────────────────────────────────────────

function renderChecks(cfg) {
  const grid = document.getElementById('checksGrid');
  grid.innerHTML = '';

  CHECK_DEFS.forEach(def => {
    const checkCfg = cfg[def.code] || { enabled: true, param1: 0, param2: 0, param3: 0 };

    // Строим поля параметров
    let paramsHtml = '';
    if (def.params.length > 0) {
      const colClass = def.params.length === 1 ? '' :
                       def.params.length === 2 ? 'row2' : 'row3';
      const inner = def.params.map(p => `
        <div class="field">
          <label>${p.label}</label>
          <input class="f-target" type="number" step="${p.step}"
                 id="check_${def.code}_${p.key}"
                 value="${checkCfg[p.key] !== undefined ? checkCfg[p.key] : 0}">
        </div>`).join('');
      paramsHtml = `<div class="${colClass}">${inner}</div>`;
    }

    const card = document.createElement('div');
    card.className = 'card';
    card.innerHTML = `
      <div class="card-title">&#9888; ${def.code} &mdash; ${def.name}</div>
      <div class="toggle-wrap">
        <label class="toggle">
          <input type="checkbox" id="check_${def.code}_enabled"
                 ${checkCfg.enabled ? 'checked' : ''}>
          <span class="slider"></span>
        </label>
        <span class="toggle-label" id="check_${def.code}_label">
          ${checkCfg.enabled ? 'Включено' : 'Выключено'}
        </span>
      </div>
      ${paramsHtml}`;
    grid.appendChild(card);

    // Обновляем подпись при переключении тоггла
    const chk = document.getElementById(`check_${def.code}_enabled`);
    const lbl = document.getElementById(`check_${def.code}_label`);
    chk.addEventListener('change', () => {
      lbl.textContent = chk.checked ? 'Включено' : 'Выключено';
    });
  });
}

function readChecks() {
  const result = {};
  CHECK_DEFS.forEach(def => {
    const enabledEl = document.getElementById(`check_${def.code}_enabled`);
    if (!enabledEl) return;
    const entry = { enabled: enabledEl.checked, param1: 0, param2: 0, param3: 0 };
    def.params.forEach(p => {
      const el = document.getElementById(`check_${def.code}_${p.key}`);
      if (el) entry[p.key] = parseFloat(el.value) || 0;
    });
    result[def.code] = entry;
  });
  return result;
}

async function loadChecks() {
  try {
    const r = await fetch('/checks');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const cfg = await r.json();
    renderChecks(cfg);
  } catch(e) {
    // Если не смогли загрузить — рендерим с дефолтными значениями
    renderChecks({});
    showToast('Ошибка загрузки проверок: ' + e.message, 'err');
  }
}

document.getElementById('btnSaveChecks').addEventListener('click', async () => {
  const btn = document.getElementById('btnSaveChecks');
  btn.disabled = true;
  try {
    const data = readChecks();
    const r = await fetch('/checks', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    });
    const text = await r.text();
    if (!r.ok) throw new Error('HTTP ' + r.status + ': ' + text);
    showToast('✓ Конфиг проверок сохранён!', 'ok');
  } catch(e) {
    showToast('Ошибка: ' + e.message, 'err');
  } finally {
    btn.disabled = false;
  }
});

// ── Alert history ──────────────────────────────────────────────────────────

async function loadAlerts() {
  try {
    const r = await fetch('/alerts');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const alerts = await r.json();
    const tbody = document.getElementById('alertTableBody');

    if (!alerts || alerts.length === 0) {
      tbody.innerHTML = '<tr><td colspan="3" class="alert-empty">Нет срабатываний</td></tr>';
      return;
    }

    tbody.innerHTML = alerts.map(a => `
      <tr>
        <td><span class="alert-code">${a.code}</span></td>
        <td class="alert-desc">${a.description}</td>
        <td class="alert-count">${a.count}</td>
      </tr>`).join('');
  } catch(e) {
    const tbody = document.getElementById('alertTableBody');
    tbody.innerHTML = '<tr><td colspan="3" class="alert-empty">Ошибка загрузки: ' + e.message + '</td></tr>';
  }
}

document.getElementById('btnClearAlerts').addEventListener('click', async () => {
  if (!confirm('Очистить всю историю алертов?')) return;
  const btn = document.getElementById('btnClearAlerts');
  btn.disabled = true;
  try {
    const r = await fetch('/alerts-clear', { method: 'POST' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    await loadAlerts();
    showToast('✓ История алертов очищена', 'ok');
  } catch(e) {
    showToast('Ошибка: ' + e.message, 'err');
  } finally {
    btn.disabled = false;
  }
});

// ── OTA ──────────────────────────────────────────────────────────────────────
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

// ── Init ──────────────────────────────────────────────────────────────────────
loadConfig();
loadChecks();
loadAlerts();
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

    // Алерты и проверки
    server_.on("/alerts",        HTTP_GET,  [this]() { handle_get_alerts(); });
    server_.on("/alerts-clear",  HTTP_POST, [this]() { handle_clear_alerts(); });
    server_.on("/checks",        HTTP_GET,  [this]() { handle_get_checks(); });
    server_.on("/checks",       HTTP_POST, [this]() { handle_post_checks(); });

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

// ── Alert handlers ────────────────────────────────────────────────────────────

void WebManager::handle_get_alerts()
{
    server_.send(200, "application/json", alert_manager.log_to_json());
}

void WebManager::handle_clear_alerts()
{
    if (alert_manager.clear_log()) {
        server_.send(200, "application/json", "{\"ok\":true}");
    } else {
        server_.send(500, "application/json", "{\"error\":\"clear failed\"}");
    }
}

void WebManager::handle_get_checks()
{
    server_.send(200, "application/json", alert_manager.checks_to_json());
}

void WebManager::handle_post_checks()
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
    Serial.printf("[Web] POST /checks  %u байт\r\n", body.length());

    if (body.isEmpty()) {
        server_.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    if (alert_manager.checks_from_json(body)) {
        server_.send(200, "application/json", "{\"ok\":true}");
    } else {
        server_.send(400, "application/json", "{\"error\":\"invalid json\"}");
    }
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
