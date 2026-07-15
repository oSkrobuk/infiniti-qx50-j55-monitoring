#include "WebManager.h"

#include "ConfigManager.h"
#include "AlertManager.h"
#include "CanBusManager.h"
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
  .nav-link {
    display: inline-block;
    margin-top: 12px;
    padding: 8px 18px;
    border: 1px solid var(--accent);
    border-radius: 8px;
    color: var(--accent);
    text-decoration: none;
    font-size: 0.8rem;
    letter-spacing: 1px;
    transition: opacity 0.2s;
  }
  .nav-link:hover { opacity: 0.75; }
  /* ── Accordion (details/summary) ─────────────────── */
  .sect {
    max-width: 960px;
    margin: 12px auto;
    border: 1px solid var(--border);
    border-radius: 10px;
    overflow: hidden;
  }
  .sect > summary {
    list-style: none;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 14px 18px;
    cursor: pointer;
    background: var(--card);
    user-select: none;
    outline: none;
  }
  .sect > summary::-webkit-details-marker { display: none; }
  .sect-title {
    font-size: 0.8rem;
    letter-spacing: 3px;
    text-transform: uppercase;
    color: var(--accent);
    font-weight: 600;
  }
  .sect-chevron {
    color: var(--muted);
    font-size: 0.85rem;
    transition: transform 0.25s;
  }
  .sect[open] > summary .sect-chevron { transform: rotate(180deg); }
  .sect-body {
    padding: 16px;
    border-top: 1px solid var(--border);
    background: var(--bg);
  }
  /* ── Cards & grid ────────────────────────────────── */
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 16px;
    margin-bottom: 16px;
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
  .row2 { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
  .row3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px; }
  .row4 { display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 8px; }
  .field label {
    display: block;
    font-size: 0.7rem;
    color: var(--muted);
    margin-bottom: 4px;
    letter-spacing: 0.5px;
    white-space: nowrap;
  }
  .field input[type="number"],
  .field input[type="text"],
  .field input[type="password"] {
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
  .field input[type="text"]:focus,
  .field input[type="password"]:focus { border-color: var(--accent); }
  .field input.f-min:focus    { border-color: var(--blue); }
  .field input.f-target:focus { border-color: var(--green); }
  .field input.f-max:focus    { border-color: var(--red); }
  /* ── Legend ─────────────────────────────────────── */
  .legend {
    display: flex;
    gap: 14px;
    font-size: 0.75rem;
    color: var(--muted);
    margin-bottom: 14px;
    flex-wrap: wrap;
  }
  .legend span { display: flex; align-items: center; gap: 4px; }
  .dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
  /* ── Actions ─────────────────────────────────────── */
  .actions {
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
    margin-top: 4px;
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
  /* ── WiFi note ───────────────────────────────────── */
  .wifi-note {
    font-size: 0.75rem;
    color: var(--muted);
    margin-top: 10px;
    line-height: 1.5;
  }
  /* ── OTA ─────────────────────────────────────────── */
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
  /* ── Toast ───────────────────────────────────────── */
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
  .toast.ok  { border-color: var(--green); color: var(--green); }
  .toast.err { border-color: var(--red);   color: var(--red); }
  /* ── Toggle switch ───────────────────────────────── */
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
  /* ── Alert table ─────────────────────────────────── */
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
  /* ── Check params ────────────────────────────────── */
  .check-params {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
  }
  .check-params .field {
    flex: 1 1 80px;
    min-width: 70px;
  }
  /* ── Card default button ────────────────────────────── */
  .card-hdr {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 14px;
  }
  .card-hdr .card-title { margin-bottom: 0; }
  .btn-card-default {
    flex: 0 0 auto;
    min-width: 0;
    padding: 4px 10px;
    font-size: 0.7rem;
    border-radius: 6px;
    background: var(--border);
    color: var(--muted);
    border: 1px solid #444;
    cursor: pointer;
    letter-spacing: 0.5px;
    white-space: nowrap;
    font-weight: 500;
  }
  .btn-card-default:hover { opacity: 0.8; }
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
  <h2>MONITORING &mdash; Редактор конфигурации</h2>
  <a class="nav-link" href="/live">&#128202; Онлайн мониторинг &rarr;</a>
</header>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 1. СИСТЕМНЫЕ ПАРАМЕТРЫ                                                  -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectSystem">
  <summary>
    <span class="sect-title">&#9881; 1. Системные параметры</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">

    <!-- WiFi -->
    <form id="wifiForm">
    <div class="grid">
      <div class="card">
        <div class="card-hdr"><div class="card-title">&#128267; WiFi &mdash; Точка доступа</div><button type="button" class="btn-card-default" onclick="resetWifiCard()">&#8635; Сброс</button></div>
        <div class="row2">
          <div class="field">
            <label>Имя сети (SSID)</label>
            <input type="text" id="wifi_ssid" name="wifi_ssid" maxlength="31" required>
          </div>
          <div class="field">
            <label>Пароль</label>
            <input type="text" id="wifi_password" name="wifi_password" maxlength="63" autocomplete="off">
          </div>
        </div>
        <p class="wifi-note">&#9888; После сохранения устройство перезагрузится. Переподключитесь к новой сети.</p>
      </div>

      <!-- Параметры CAN-опроса -->
      <div class="card">
        <div class="card-hdr"><div class="card-title">&#9201; CAN &mdash; Параметры опроса</div><button type="button" class="btn-card-default" onclick="resetCardFields('system')">&#8635; Сброс</button></div>
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
      <button type="button" class="btn-default" onclick="resetAllSystem()">&#8635; Сбросить все</button>
      <button type="submit" class="btn-save" id="btnSaveWifi">&#10003; Сохранить WiFi &amp; систему</button>
    </div>
    </form>

  </div>
</details>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 2. ИСТОРИЯ СРАБАТЫВАНИЙ                                                 -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectAlerts">
  <summary>
    <span class="sect-title">&#128680; 2. История срабатываний</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">
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
</details>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 3. КОНФИГУРАЦИЯ ПРОВЕРОК                                                -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectChecks">
  <summary>
    <span class="sect-title">&#9888; 3. Конфигурация проверок</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">
    <div class="grid" id="checksGrid">
      <!-- Заполняется JavaScript -->
    </div>
    <div class="actions">
      <button type="button" class="btn-default" onclick="resetAllChecks()">&#8635; Сбросить все</button>
      <button type="button" class="btn-save" id="btnSaveChecks">&#10003; Сохранить проверки</button>
    </div>
  </div>
</details>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 4. КОНФИГУРАЦИЯ МЕТРИК                                                  -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectMetrics">
  <summary>
    <span class="sect-title">&#128202; 4. Конфигурация метрик</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">

    <div class="legend">
      <span><span class="dot" style="background:#2196f3"></span> Минимум</span>
      <span><span class="dot" style="background:#4caf50"></span> Целевая</span>
      <span><span class="dot" style="background:#f44336"></span> Максимум</span>
    </div>

    <form id="configForm">
    <div class="grid">

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#128167; E-OIL &mdash; Моторное масло</div><button type="button" class="btn-card-default" onclick="resetCardFields('oil')">&#8635; Сброс</button></div>
        <div class="row3">
          <div class="field">
            <label>Мин, °C</label>
            <input class="f-min" type="number" step="0.01" name="oil_min" required>
          </div>
          <div class="field">
            <label>Цель, °C</label>
            <input class="f-target" type="number" step="0.01" name="oil_target" required>
          </div>
          <div class="field">
            <label>Макс, °C</label>
            <input class="f-max" type="number" step="0.01" name="oil_max" required>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#10052; E-COOL &mdash; Антифриз ДВС</div><button type="button" class="btn-card-default" onclick="resetCardFields('coolant')">&#8635; Сброс</button></div>
        <div class="row3">
          <div class="field">
            <label>Мин, °C</label>
            <input class="f-min" type="number" step="0.01" name="coolant_min" required>
          </div>
          <div class="field">
            <label>Цель, °C</label>
            <input class="f-target" type="number" step="0.01" name="coolant_target" required>
          </div>
          <div class="field">
            <label>Макс, °C</label>
            <input class="f-max" type="number" step="0.01" name="coolant_max" required>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#127777; R-COOL &mdash; Антифриз радиатора</div><button type="button" class="btn-card-default" onclick="resetCardFields('radiator')">&#8635; Сброс</button></div>
        <div class="row3">
          <div class="field">
            <label>Мин, °C</label>
            <input class="f-min" type="number" step="0.01" name="radiator_min" required>
          </div>
          <div class="field">
            <label>Цель, °C</label>
            <input class="f-target" type="number" step="0.01" name="radiator_target" required>
          </div>
          <div class="field">
            <label>Макс, °C</label>
            <input class="f-max" type="number" step="0.01" name="radiator_max" required>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#9881; T-OIL &mdash; Масло АКПП</div><button type="button" class="btn-card-default" onclick="resetCardFields('transmission')">&#8635; Сброс</button></div>
        <div class="row3">
          <div class="field">
            <label>Мин, °C</label>
            <input class="f-min" type="number" step="0.01" name="transmission_min" required>
          </div>
          <div class="field">
            <label>Цель, °C</label>
            <input class="f-target" type="number" step="0.01" name="transmission_target" required>
          </div>
          <div class="field">
            <label>Макс, °C</label>
            <input class="f-max" type="number" step="0.01" name="transmission_max" required>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#9889; RPM &mdash; Обороты двигателя</div><button type="button" class="btn-card-default" onclick="resetCardFields('rpm')">&#8635; Сброс</button></div>
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
        <div class="card-hdr"><div class="card-title">&#128167; EOP &mdash; Давление масла</div><button type="button" class="btn-card-default" onclick="resetCardFields('oil_pressure')">&#8635; Сброс</button></div>
        <div class="row3">
          <div class="field">
            <label>Порог RPM</label>
            <input class="f-target" type="number" step="100" name="oil_pressure_rpm_threshold" required>
          </div>
          <div class="field">
            <label>Мин &lt;порога, бар</label>
            <input class="f-min" type="number" step="0.01" name="oil_pressure_min_low" required>
          </div>
          <div class="field">
            <label>Мин &ge;порога, бар</label>
            <input class="f-max" type="number" step="0.01" name="oil_pressure_min_high" required>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#128168; BOOST &mdash; Давление наддува</div><button type="button" class="btn-card-default" onclick="resetCardFields('boost')">&#8635; Сброс</button></div>
        <div class="row2">
          <div class="field">
            <label>Синий до, В</label>
            <input class="f-min" type="number" step="0.01" name="boost_blue_max" required>
          </div>
          <div class="field">
            <label>Зелёный от, В</label>
            <input class="f-target" type="number" step="0.01" name="boost_green_min" required>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#128267; BATTERY &mdash; Бортовая сеть</div><button type="button" class="btn-card-default" onclick="resetCardFields('battery')">&#8635; Сброс</button></div>
        <div class="row4">
          <div class="field">
            <label>Красный &lt;, В</label>
            <input class="f-min" type="number" step="0.01" name="battery_red_low" required>
          </div>
          <div class="field">
            <label>Зелёный от, В</label>
            <input class="f-target" type="number" step="0.01" name="battery_green_min" required>
          </div>
          <div class="field">
            <label>Зелёный до, В</label>
            <input class="f-target" type="number" step="0.01" name="battery_green_max" required>
          </div>
          <div class="field">
            <label>Красный &gt;, В</label>
            <input class="f-max" type="number" step="0.01" name="battery_red_high" required>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-hdr"><div class="card-title">&#9201; RPM-POLL &mdash; Время опроса RPM</div><button type="button" class="btn-card-default" onclick="resetCardFields('poll_time')">&#8635; Сброс</button></div>
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

    </div>

    <div class="actions">
      <button type="button" class="btn-default" id="btnDefault">&#8635; Сбросить все</button>
      <button type="submit" class="btn-save" id="btnSave">&#10003; Сохранить метрики</button>
    </div>
    </form>

  </div>
</details>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 5. ОБНОВЛЕНИЕ ПРОШИВКИ                                                  -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectOta">
  <summary>
    <span class="sect-title">&#8593; 5. Обновление прошивки</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">
    <div class="card">
      <div class="card-title">&#8593; OTA &mdash; Загрузка .bin</div>
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
</details>

<div class="toast" id="toast"></div>

<footer>Infiniti QX50 J55 Monitoring &mdash; ESP32</footer>

<script>
// ── Утилиты ────────────────────────────────────────────────────────────────

function showToast(msg, type) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast ' + type + ' show';
  setTimeout(() => { t.className = 'toast'; }, 5000);
}

// ── Конфиг метрик ──────────────────────────────────────────────────────────

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

function readMetricsForm() {
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
  return d;
}

function validateMetrics(data) {
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
  return null;
}

async function loadConfig() {
  try {
    const r = await fetch('/config');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const cfg = await r.json();
    fillForm(cfg);
  } catch(e) {
    showToast('Ошибка загрузки конфига: ' + e.message, 'err');
  }
}

document.getElementById('configForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const data = readMetricsForm();
  const err = validateMetrics(data);
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
    showToast('✓ Метрики сохранены!', 'ok');
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
    fillForm(cfg);
    showToast('Значения по умолчанию восстановлены', 'ok');
  } catch(e) {
    showToast('Ошибка: ' + e.message, 'err');
  } finally {
    btn.disabled = false;
  }
});

// ── WiFi + система ─────────────────────────────────────────────────────────

async function loadWifi() {
  try {
    const r = await fetch('/wifi');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const w = await r.json();
    const ssidEl = document.getElementById('wifi_ssid');
    const passEl = document.getElementById('wifi_password');
    if (ssidEl) ssidEl.value = w.ssid || '';
    if (passEl) passEl.value = w.password || '';
  } catch(e) {
    showToast('Ошибка загрузки WiFi: ' + e.message, 'err');
  }
}

document.getElementById('wifiForm').addEventListener('submit', async (e) => {
  e.preventDefault();

  const ssid = document.getElementById('wifi_ssid').value.trim();
  const pass  = document.getElementById('wifi_password').value;
  if (!ssid) { showToast('⚠ SSID не может быть пустым', 'err'); return; }

  const pollEl = document.querySelector('[name="system_poll_interval_ms"]');
  const staleEl = document.querySelector('[name="system_stale_ms"]');
  const poll_ms  = parseFloat(pollEl  ? pollEl.value  : 30);
  const stale_ms = parseFloat(staleEl ? staleEl.value : 1000);

  if (poll_ms < 10)  { showToast('⚠ Интервал опроса не может быть меньше 10 мс', 'err'); return; }
  if (stale_ms < 100){ showToast('⚠ Порог устаревания не может быть меньше 100 мс', 'err'); return; }
  if (poll_ms >= stale_ms){ showToast('⚠ Интервал опроса должен быть меньше порога устаревания', 'err'); return; }

  const btn = document.getElementById('btnSaveWifi');
  btn.disabled = true;

  // Сохраняем WiFi
  try {
    const payload = { wifi: { ssid: ssid, password: pass } };
    const r = await fetch('/wifi', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const text = await r.text();
    if (!r.ok) throw new Error('HTTP ' + r.status + ': ' + text);
  } catch(e) {
    showToast('Ошибка сохранения WiFi: ' + e.message, 'err');
    btn.disabled = false;
    return;
  }

  // Сохраняем системные параметры CAN
  try {
    const sysPayload = { system: { poll_interval_ms: poll_ms, stale_ms: stale_ms } };
    const r2 = await fetch('/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(sysPayload)
    });
    if (!r2.ok) throw new Error('HTTP ' + r2.status);
  } catch(e) {
    showToast('Ошибка сохранения системных параметров: ' + e.message, 'err');
    btn.disabled = false;
    return;
  }

  showToast('✓ Сохранено. Перезагрузка...', 'ok');
  setTimeout(async () => {
    try { await fetch('/restart', { method: 'POST' }); } catch(_) {}
  }, 1200);
});

// ── Проверки ───────────────────────────────────────────────────────────────

const CHECK_DEFS = [
  { code:'E01', name:'Engine Oil Temp High',
    desc:'Температура масла двигателя превысила максимальный порог',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E02', name:'Engine Coolant Temp High',
    desc:'Температура антифриза двигателя превысила максимальный порог',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E03', name:'Radiator Coolant Temp High',
    desc:'Температура антифриза радиатора превысила максимальный порог',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E04', name:'CVT Oil Temp High',
    desc:'Температура масла вариатора превысила максимальный порог',
    params:[{label:'Макс. темп, °C', key:'param1', step:1}] },
  { code:'E05', name:'Engine RPM Overspeed',
    desc:'Обороты двигателя превысили максимально допустимый предел',
    params:[{label:'Макс. об/мин', key:'param1', step:50}] },
  { code:'E06', name:'Battery Voltage Low',
    desc:'Напряжение бортовой сети упало ниже минимального порога',
    params:[{label:'Мин. напряжение, В', key:'param1', step:0.01}] },
  { code:'E07', name:'Battery Voltage High',
    desc:'Напряжение бортовой сети превысило максимальный порог',
    params:[{label:'Макс. напряжение, В', key:'param1', step:0.01}] },
  { code:'E08', name:'Oil Pressure Low',
    desc:'Напряжение датчика давления масла ниже нормы для текущих оборотов',
    params:[
      {label:'Порог об/мин', key:'param1', step:100},
      {label:'Мин. В (низк. обор.)', key:'param2', step:0.01},
      {label:'Мин. В (выс. обор.)', key:'param3', step:0.01}
    ]
  },
  { code:'E09', name:'Oil-Coolant Temp Delta High',
    desc:'Разница температур масла и антифриза двигателя превысила допустимый порог',
    params:[{label:'Макс. дельта, °C', key:'param1', step:1}] },
];

function renderChecks(cfg) {
  const grid = document.getElementById('checksGrid');
  grid.innerHTML = '';

  CHECK_DEFS.forEach(def => {
    const checkCfg = cfg[def.code] || { enabled: true, param1: 0, param2: 0, param3: 0 };

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
      <div class="card-hdr"><div class="card-title">&#9888; ${def.code} &mdash; ${def.name}</div><button type="button" class="btn-card-default" onclick="resetCheckCard('${def.code}')">&#8635; Сброс</button></div>
      <div style="font-size:0.75rem;color:var(--muted);margin-bottom:10px;line-height:1.4">${def.desc}</div>
      <div class="toggle-wrap" style="margin-bottom:12px;">
        <label class="toggle">
          <input type="checkbox" id="check_${def.code}_enabled"
                 ${checkCfg.enabled ? 'checked' : ''}>
          <span class="slider"></span>
        </label>
        <span class="toggle-label" id="check_${def.code}_label">
            ${checkCfg.enabled ? 'При постоянной ошибки повтор раз в 15 сек' : 'Однократно за сессию'}
          </span>
      </div>
      ${paramsHtml}`;
    grid.appendChild(card);

    const chk = document.getElementById(`check_${def.code}_enabled`);
    const lbl = document.getElementById(`check_${def.code}_label`);
    chk.addEventListener('change', () => {
      lbl.textContent = chk.checked ? 'При постоянной ошибки повтор раз в 15 сек' : 'Однократно за сессию';
    });
  });
}

function readChecks() {
  const result = {};
  CHECK_DEFS.forEach(def => {
    const enabledEl = document.getElementById(`check_${def.code}_enabled`);
    if (!enabledEl) return;
    const entry = {
      enabled: enabledEl.checked,
      param1: 0, param2: 0, param3: 0
    };
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

// ── История алертов ────────────────────────────────────────────────────────

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

// Открываем секцию с историей при загрузке (для удобства навигации)
document.getElementById('sectAlerts').addEventListener('toggle', function() {
  if (this.open) loadAlerts();
});

// ── OTA ────────────────────────────────────────────────────────────────────

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

// ── Сброс карточек к значениям по умолчанию ───────────────────────────────

// Значения по умолчанию совпадают с build_defaults() в ConfigManager.cpp
const CARD_DEFAULTS = {
  wifi:         { ssid: 'QX50Monitoring', password: 'infiniti' },
  system:       { poll_interval_ms: 30, stale_ms: 1000 },
  oil:          { min: 50, target: 90, max: 98 },
  coolant:      { min: 50, target: 90, max: 93 },
  radiator:     { min: 0,  target: 50, max: 90 },
  transmission: { min: 50, target: 80, max: 98 },
  rpm:          { green_start: 1000, green_end: 3500, red_start: 4500 },
  oil_pressure: { rpm_threshold: 3000, min_low: 1.45, min_high: 3.1 },
  boost:        { blue_max: 1.3, green_min: 1.58 },
  battery:      { red_low: 11.5, green_min: 12.0, green_max: 14.6, red_high: 14.9 },
  poll_time:    { green_max: 0.2, red_min: 0.5 },
};

// Дефолты проверок совпадают с CHECK_DEFS в AlertManager.cpp
const CHECK_DEFAULTS = {
  E01: { enabled: true, param1: 98,   param2: 0,   param3: 0   },
  E02: { enabled: true, param1: 96,   param2: 0,   param3: 0   },
  E03: { enabled: true, param1: 90,   param2: 0,   param3: 0   },
  E04: { enabled: true, param1: 100,  param2: 0,   param3: 0   },
  E05: { enabled: true, param1: 6500, param2: 0,   param3: 0   },
  E06: { enabled: true, param1: 11.5, param2: 0,   param3: 0   },
  E07: { enabled: true, param1: 15,   param2: 0,   param3: 0   },
  E08: { enabled: true, param1: 3000, param2: 1.4, param3: 2.9 },
  E09: { enabled: true, param1: 14,   param2: 0,   param3: 0   },
};

// Сбрасывает поля числовой карточки к дефолтам по имени секции
function resetCardFields(section) {
  const d = CARD_DEFAULTS[section];
  if (!d) return;
  Object.entries(d).forEach(([key, val]) => {
    const el = document.querySelector(`[name="${section}_${key}"]`);
    if (el) el.value = val;
  });
}

// Сбрасывает все поля раздела «Системные параметры» к дефолтам
function resetAllSystem() {
  resetWifiCard();
  resetCardFields('system');
}

// Сбрасывает все карточки проверок к дефолтам
function resetAllChecks() {
  Object.keys(CHECK_DEFAULTS).forEach(code => resetCheckCard(code));
}

// Сбрасывает WiFi-карточку к дефолтам
function resetWifiCard() {
  const d = CARD_DEFAULTS.wifi;
  const ssidEl = document.getElementById('wifi_ssid');
  const passEl = document.getElementById('wifi_password');
  if (ssidEl) ssidEl.value = d.ssid;
  if (passEl) passEl.value = d.password;
}

// Сбрасывает карточку проверки к дефолтам
function resetCheckCard(code) {
  const d = CHECK_DEFAULTS[code];
  if (!d) return;
  const enabledEl = document.getElementById(`check_${code}_enabled`);
  const labelEl   = document.getElementById(`check_${code}_label`);
  if (enabledEl) {
    enabledEl.checked = d.enabled;
    if (labelEl) labelEl.textContent = d.enabled ? 'При постоянной ошибки повтор раз в 15 сек' : 'Однократно за сессию';
  }
  ['param1', 'param2', 'param3'].forEach(p => {
    const el = document.getElementById(`check_${code}_${p}`);
    if (el && d[p] !== undefined) el.value = d[p];
  });
}

// ── Init ───────────────────────────────────────────────────────────────────

loadConfig();
loadWifi();
loadChecks();
// Алерты загружаются при открытии секции (toggle event), но при первом открытии
// секция может быть уже открыта — грузим сразу
loadAlerts();
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────
// Страница онлайн мониторинга (GET /live) — хранится во флеш (PROGMEM)

static const char LIVE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>QX50 J55 — Метрики</title>
<style>
  :root {
    --bg: #0d0d0d;
    --card: #1a1a1a;
    --border: #2a2a2a;
    --accent: #c9a84c;
    --text: #e0e0e0;
    --muted: #888;
    --cold: #2196f3;
    --ok: #4caf50;
    --warn: #e0b020;
    --danger: #f44336;
    --nodata: #555;
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
    padding: 20px 0 8px;
    border-bottom: 1px solid var(--border);
    margin-bottom: 16px;
  }
  header h1 {
    font-size: 1.3rem;
    letter-spacing: 3px;
    color: var(--accent);
    text-transform: uppercase;
  }
  .nav-link {
    display: inline-block;
    margin-top: 12px;
    padding: 8px 18px;
    border: 1px solid var(--border);
    border-radius: 8px;
    color: var(--muted);
    text-decoration: none;
    font-size: 0.8rem;
    letter-spacing: 1px;
    transition: opacity 0.2s;
  }
  .nav-link:hover { opacity: 0.75; }
  /* ── Статус-строка ─────────────────────────────── */
  .status {
    max-width: 960px;
    margin: 0 auto 16px;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    font-size: 0.8rem;
    color: var(--muted);
    letter-spacing: 0.5px;
  }
  .status-dot {
    width: 9px;
    height: 9px;
    border-radius: 50%;
    background: var(--nodata);
    transition: background 0.3s;
  }
  .status.online .status-dot { background: var(--ok); }
  .status.offline .status-dot { background: var(--danger); }
  /* ── Сетка карточек ────────────────────────────── */
  .grid {
    max-width: 960px;
    margin: 0 auto;
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 14px;
  }
  .mcard {
    background: var(--card);
    border: 1px solid var(--border);
    border-left: 4px solid var(--nodata);
    border-radius: 10px;
    padding: 14px 16px;
    transition: border-color 0.3s;
  }
  .m-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .m-label {
    font-size: 0.75rem;
    letter-spacing: 2px;
    text-transform: uppercase;
    color: var(--accent);
    font-weight: 600;
  }
  .m-name {
    font-size: 0.72rem;
    color: var(--muted);
    margin-top: 2px;
  }
  .m-val {
    margin-top: 12px;
    display: flex;
    align-items: baseline;
    gap: 6px;
  }
  .m-num {
    font-size: 2rem;
    font-weight: 700;
    font-variant-numeric: tabular-nums;
    color: var(--text);
    transition: color 0.3s;
  }
  .m-unit {
    font-size: 0.85rem;
    color: var(--muted);
  }
  /* Цвета по состоянию */
  .mcard.st-cold   { border-left-color: var(--cold); }
  .mcard.st-ok     { border-left-color: var(--ok); }
  .mcard.st-warn   { border-left-color: var(--warn); }
  .mcard.st-danger { border-left-color: var(--danger); }
  .mcard.st-nodata { border-left-color: var(--nodata); }
  .mcard.st-cold   .m-num { color: var(--cold); }
  .mcard.st-ok     .m-num { color: var(--ok); }
  .mcard.st-warn   .m-num { color: var(--warn); }
  .mcard.st-danger .m-num { color: var(--danger); }
  .mcard.st-nodata .m-num { color: var(--nodata); }
  /* ── Баннер ошибок ─────────────────────────────── */
  .err-banner {
    max-width: 960px;
    margin: 16px auto 0;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    padding: 12px 18px;
    background: rgba(244, 67, 54, 0.12);
    border: 1px solid var(--danger);
    border-radius: 10px;
    color: var(--danger);
    font-size: 0.85rem;
    font-weight: 600;
    letter-spacing: 0.5px;
  }
  .err-banner[hidden] { display: none; }
  /* ── История срабатываний ──────────────────────── */
  .alerts {
    max-width: 960px;
    margin: 28px auto 0;
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 16px 18px;
  }
  .alerts-hdr {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    margin-bottom: 12px;
  }
  .alerts-title {
    font-size: 0.8rem;
    letter-spacing: 2px;
    text-transform: uppercase;
    color: var(--accent);
    font-weight: 600;
  }
  .btn-clear-alerts {
    flex: 0 0 auto;
    padding: 8px 16px;
    font-size: 0.8rem;
    background: transparent;
    color: var(--danger);
    border: 1px solid var(--danger);
    border-radius: 8px;
    cursor: pointer;
    transition: opacity 0.2s;
  }
  .btn-clear-alerts:hover { opacity: 0.75; }
  .btn-clear-alerts:disabled { opacity: 0.4; cursor: default; }
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
    font-size: 0.72rem;
    letter-spacing: 1px;
    text-transform: uppercase;
  }
  .alert-table td {
    padding: 7px 8px;
    border-bottom: 1px solid #1c1c1c;
    vertical-align: top;
  }
  .alert-code {
    color: var(--danger);
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
  /* ── Toast ───────────────────────────────────────── */
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
    text-align: center;
    visibility: hidden;
  }
  .toast.show {
    transform: translateX(-50%) translateY(0);
    visibility: visible;
    transition: transform 0.4s ease, visibility 0s linear 0s;
  }
  .toast.ok  { border-color: var(--ok);     color: var(--ok); }
  .toast.err { border-color: var(--danger); color: var(--danger); }
  footer {
    text-align: center;
    color: var(--muted);
    font-size: 0.75rem;
    margin-top: 32px;
    letter-spacing: 1px;
  }
</style>
</head>
<body>

<header>
  <h1>&#128202; Онлайн мониторинг</h1>
  <a class="nav-link" href="/">&larr; К настройкам</a>
</header>

<div class="status" id="status">
  <span class="status-dot"></span>
  <span id="statusText">Подключение...</span>
</div>

<div class="grid" id="grid"></div>

<div class="err-banner" id="errBanner" hidden>
  <span>&#9888;</span>
  <span id="errBannerText"></span>
</div>

<div class="alerts">
  <div class="alerts-hdr">
    <span class="alerts-title">&#128680; История срабатываний</span>
    <button type="button" class="btn-clear-alerts" id="btnClearAlerts">
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

<div class="toast" id="toast"></div>

<footer>Infiniti QX50 J55 Monitoring &mdash; ESP32</footer>

<script>
// Интервал опроса метрик, мс
const POLL_MS = 500;

const grid       = document.getElementById('grid');
const statusEl   = document.getElementById('status');
const statusText = document.getElementById('statusText');
const cards      = {}; // key -> {card, num, unit}

// Форматировать время работы устройства (мс -> ЧЧ:ММ:СС)
function fmtUptime(ms) {
  let s = Math.floor(ms / 1000);
  const h = Math.floor(s / 3600); s -= h * 3600;
  const m = Math.floor(s / 60);   s -= m * 60;
  const pad = n => String(n).padStart(2, '0');
  return pad(h) + ':' + pad(m) + ':' + pad(s);
}

// Создать карточку метрики при первом появлении ключа
function ensureCard(m) {
  if (cards[m.key]) return cards[m.key];
  const card = document.createElement('div');
  card.className = 'mcard st-nodata';
  card.innerHTML =
    '<div class="m-top"><span class="m-label"></span></div>' +
    '<div class="m-name"></div>' +
    '<div class="m-val"><span class="m-num">&mdash;</span><span class="m-unit"></span></div>';
  grid.appendChild(card);
  const o = {
    card: card,
    num:  card.querySelector('.m-num'),
    unit: card.querySelector('.m-unit'),
  };
  card.querySelector('.m-label').textContent = m.label;
  card.querySelector('.m-name').textContent  = m.name;
  o.unit.textContent = m.unit;
  cards[m.key] = o;
  return o;
}

// Отрисовать очередной ответ /metrics
function render(data) {
  data.metrics.forEach(m => {
    const o = ensureCard(m);
    o.num.textContent = m.fresh ? m.disp : '—';
    o.unit.style.visibility = m.fresh ? 'visible' : 'hidden';
    o.card.className = 'mcard st-' + (m.fresh ? m.state : 'nodata');
  });
}

// Обновить индикатор связи
function setOnline(on, uptimeMs) {
  statusEl.className = 'status ' + (on ? 'online' : 'offline');
  if (on) {
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    const t = pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' + pad(now.getSeconds());
    statusText.textContent = 'В сети · обновлено ' + t + ' · аптайм ' + fmtUptime(uptimeMs);
  } else {
    statusText.textContent = 'Нет связи с устройством';
  }
}

// Цикл опроса (setTimeout вместо setInterval — без наложения запросов)
async function tick() {
  try {
    const r = await fetch('/metrics', { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const data = await r.json();
    render(data);
    setOnline(true, data.uptime_ms);
  } catch (e) {
    setOnline(false);
  } finally {
    setTimeout(tick, POLL_MS);
  }
}

// ── История срабатываний ────────────────────────────────────────────────────

// Интервал обновления истории алертов, мс
const ALERTS_MS = 5000;

function showToast(msg, type) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast ' + type + ' show';
  setTimeout(() => { t.className = 'toast'; }, 5000);
}

// Показать/скрыть баннер ошибок по наличию записей в истории
function updateErrBanner(alerts) {
  const banner = document.getElementById('errBanner');
  const txt    = document.getElementById('errBannerText');
  if (alerts && alerts.length > 0) {
    const total = alerts.reduce((s, a) => s + (a.count || 0), 0);
    txt.textContent = 'Зафиксированы ошибки: ' + alerts.length +
      ' (срабатываний: ' + total + ')';
    banner.hidden = false;
  } else {
    banner.hidden = true;
  }
}

async function loadAlerts() {
  try {
    const r = await fetch('/alerts', { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const alerts = await r.json();
    updateErrBanner(alerts);
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

// Периодическое обновление истории (setTimeout — без наложения запросов)
async function alertsTick() {
  await loadAlerts();
  setTimeout(alertsTick, ALERTS_MS);
}

tick();
alertsTick();
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────
// Расчёт состояния метрики по порогам конфига — зеркалит логику зон DisplayManager,
// но возвращает строковый код состояния для веб-интерфейса:
// "cold" (синий), "ok" (зелёный), "warn" (жёлтый), "danger" (красный), "nodata" (серый)

// true если значение получено не позднее stale_ms мс назад
static bool metric_fresh(uint32_t ts, uint32_t stale_ms)
{
    if (ts == 0) return false;
    return (millis() - ts) <= stale_ms;
}

// Состояние температурной метрики по порогам min/target/max
static const char *temp_state(float v, float mn, float tgt, float mx)
{
    if (v >= mx)  return "danger";
    if (v <= mn)  return "cold";
    if (v >= tgt) return "warn";
    return "ok";
}

// Состояние оборотов двигателя
static const char *rpm_state(float v)
{
    const float green_start = config.get("rpm", "green_start");
    const float green_end   = config.get("rpm", "green_end");
    const float red_start   = config.get("rpm", "red_start");
    if (v >= red_start)   return "danger";
    if (v <= green_start) return "cold";
    if (v <= green_end)   return "ok";
    return "warn";
}

// Состояние давления масла — минимум зависит от текущих оборотов
static const char *oil_pressure_state(float v, float rpm)
{
    const float threshold = config.get("oil_pressure", "rpm_threshold");
    const float min_p = (rpm < threshold)
        ? config.get("oil_pressure", "min_low")
        : config.get("oil_pressure", "min_high");
    return (v < min_p) ? "danger" : "ok";
}

// Состояние давления наддува турбины
static const char *boost_state(float v)
{
    const float blue_max  = config.get("boost", "blue_max");
    const float green_min = config.get("boost", "green_min");
    if (v <= blue_max)  return "cold";
    if (v >= green_min) return "ok";
    return "warn";
}

// Состояние напряжения бортовой сети
static const char *battery_state(float v)
{
    const float red_low   = config.get("battery", "red_low");
    const float green_min = config.get("battery", "green_min");
    const float green_max = config.get("battery", "green_max");
    const float red_high  = config.get("battery", "red_high");
    if (v < red_low || v > red_high)              return "danger";
    if (v >= green_min && v <= green_max)         return "ok";
    return "warn";
}

// Состояние периода опроса RPM
static const char *poll_time_state(float v, float rpm)
{
    if (rpm == 0.0f || v == 0.0f) return "cold";
    const float green_max = config.get("poll_time", "green_max");
    const float red_min   = config.get("poll_time", "red_min");
    if (v <= green_max) return "ok";
    if (v >= red_min)   return "danger";
    return "warn";
}

// Добавить один объект метрики в JSON-массив (disp — уже отформатированное значение)
static void append_metric(String &out, const char *key, const char *label,
                          const char *name, const char *unit, float value,
                          int precision, bool fresh, const char *state)
{
    char num[16];
    snprintf(num, sizeof(num), "%.*f", precision, value);

    // Запятая-разделитель между объектами (перед новым объектом уже есть '}')
    if (out.length() > 0 && out[out.length() - 1] == '}') out += ',';

    out += "{\"key\":\"";     out += key;
    out += "\",\"label\":\""; out += label;
    out += "\",\"name\":\"";  out += name;
    out += "\",\"unit\":\"";  out += unit;
    out += "\",\"disp\":\"";  out += num;
    out += "\",\"fresh\":";   out += (fresh ? "true" : "false");
    out += ",\"state\":\"";   out += state;
    out += "\"}";
}

// ─────────────────────────────────────────────

WebManager::WebManager()
    : server_(80)
{
}

void WebManager::begin()
{
    // WiFi ssid/password читаются из конфига (установлены в build_defaults или сохранены пользователем)
    String ssid = config.get_str("wifi", "ssid");
    String pass = config.get_str("wifi", "password");

    if (ssid.isEmpty()) ssid = "QX50Monitoring";

    WiFi.softAP(ssid.c_str(), pass.isEmpty() ? nullptr : pass.c_str());
    Serial.printf("[WiFi] AP запущен  SSID: %s\r\n", ssid.c_str());

    server_.on("/",            HTTP_GET,  [this]() { handle_root(); });
    server_.on("/live",        HTTP_GET,  [this]() { handle_live_page(); });
    server_.on("/metrics",     HTTP_GET,  [this]() { handle_get_metrics(); });
    server_.on("/config",      HTTP_GET,  [this]() { handle_get_config(); });
    server_.on("/config",      HTTP_POST, [this]() { handle_post_config(); });
    server_.on("/reset",       HTTP_POST, [this]() { handle_reset(); });
    server_.on("/favicon.ico", HTTP_GET,  [this]() { server_.send(204); });

    // WiFi настройки
    server_.on("/wifi",        HTTP_GET,  [this]() { handle_get_wifi(); });
    server_.on("/wifi",        HTTP_POST, [this]() { handle_post_wifi(); });

    // Перезагрузка (вызывается из JS после сохранения WiFi)
    server_.on("/restart", HTTP_POST, [this]() {
        server_.send(200, "application/json", "{\"ok\":true}");
        Serial.println("[Web] Перезагрузка по запросу из веб-интерфейса");
        delay(300);
        ESP.restart();
    });

    // Алерты и проверки
    server_.on("/alerts",        HTTP_GET,  [this]() { handle_get_alerts(); });
    server_.on("/alerts-clear",  HTTP_POST, [this]() { handle_clear_alerts(); });
    server_.on("/checks",        HTTP_GET,  [this]() { handle_get_checks(); });
    server_.on("/checks",        HTTP_POST, [this]() { handle_post_checks(); });

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

void WebManager::handle_live_page()
{
    server_.send_P(200, "text/html; charset=utf-8", LIVE_HTML);
}

void WebManager::handle_get_metrics()
{
    const uint32_t stale_ms = static_cast<uint32_t>(config.get("system", "stale_ms"));

    // Обороты нужны для расчёта состояния давления масла и периода опроса
    const bool  rpm_fresh = metric_fresh(can_metrics.engine_rpm_ts, stale_ms);
    const float rpm       = rpm_fresh ? can_metrics.engine_rpm : 0.0f;

    String json;
    json.reserve(1600);
    json += "{\"uptime_ms\":";
    json += String(millis());
    json += ",\"stale_ms\":";
    json += String(stale_ms);
    json += ",\"metrics\":[";

    // ── Температуры ──
    {
        const bool f = metric_fresh(can_metrics.radiator_coolant_ts, stale_ms);
        append_metric(json, "radiator", "RAD-ANT", "Антифриз радиатора", "°C",
            can_metrics.radiator_coolant, 0, f, f ? temp_state(can_metrics.radiator_coolant,
                config.get("radiator", "min"), config.get("radiator", "target"),
                config.get("radiator", "max")) : "nodata");
    }
    {
        const bool f = metric_fresh(can_metrics.engine_coolant_ts, stale_ms);
        append_metric(json, "coolant", "ENG-ANT", "Антифриз ДВС", "°C",
            can_metrics.engine_coolant, 0, f, f ? temp_state(can_metrics.engine_coolant,
                config.get("coolant", "min"), config.get("coolant", "target"),
                config.get("coolant", "max")) : "nodata");
    }
    {
        const bool f = metric_fresh(can_metrics.engine_oil_ts, stale_ms);
        append_metric(json, "oil", "ENG-OIL", "Масло ДВС", "°C",
            can_metrics.engine_oil, 0, f, f ? temp_state(can_metrics.engine_oil,
                config.get("oil", "min"), config.get("oil", "target"),
                config.get("oil", "max")) : "nodata");
    }

    // ── Двигатель ──
    append_metric(json, "rpm", "ENG-RPM", "Обороты двигателя", "об/мин",
        can_metrics.engine_rpm, 0, rpm_fresh, rpm_fresh ? rpm_state(can_metrics.engine_rpm) : "nodata");
    {
        const bool f = metric_fresh(can_metrics.oil_pressure_volt_ts, stale_ms);
        append_metric(json, "oil_pressure", "OIL-PR-V", "Давление масла", "В",
            can_metrics.oil_pressure_volt, 2, f,
            f ? oil_pressure_state(can_metrics.oil_pressure_volt, rpm) : "nodata");
    }
    {
        const bool f = metric_fresh(can_metrics.turbo_boost_volt_ts, stale_ms);
        append_metric(json, "boost", "TURBO-V", "Наддув турбины", "В",
            can_metrics.turbo_boost_volt, 2, f,
            f ? boost_state(can_metrics.turbo_boost_volt) : "nodata");
    }

    // ── Прочее ──
    {
        const bool f = metric_fresh(can_metrics.battery_voltage_ts, stale_ms);
        append_metric(json, "battery", "BATTERY-V", "Бортовая сеть", "В",
            can_metrics.battery_voltage, 2, f,
            f ? battery_state(can_metrics.battery_voltage) : "nodata");
    }
    {
        // Период опроса RPM не ограничиваем stale_ms — это результат последнего замера,
        // а не живой поток (см. main.cpp)
        const bool f = (can_metrics.rpm_poll_time_ts != 0);
        append_metric(json, "poll_time", "RPM-POLL", "Период опроса RPM", "с",
            can_metrics.rpm_poll_time, 2, f,
            f ? poll_time_state(can_metrics.rpm_poll_time, rpm) : "nodata");
    }
    {
        const bool f = metric_fresh(can_metrics.cvt_temp_ts, stale_ms);
        append_metric(json, "cvt", "CVT-FLD", "Масло вариатора", "°C",
            can_metrics.cvt_temp, 0, f, f ? temp_state(can_metrics.cvt_temp,
                config.get("transmission", "min"), config.get("transmission", "target"),
                config.get("transmission", "max")) : "nodata");
    }

    json += "]}";
    server_.send(200, "application/json", json);
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

// ── WiFi handlers ─────────────────────────────────────────────────────────────

void WebManager::handle_get_wifi()
{
    String ssid = config.get_str("wifi", "ssid");
    String pass = config.get_str("wifi", "password");
    String json = "{\"ssid\":\"" + ssid + "\",\"password\":\"" + pass + "\"}";
    server_.send(200, "application/json", json);
}

void WebManager::handle_post_wifi()
{
    String body;

    if (server_.hasArg("plain")) {
        body = server_.arg("plain");
    }

    if (body.isEmpty()) {
        WiFiClient client = server_.client();
        int len = server_.header("Content-Length").toInt();
        if (len > 0 && len < 512) {
            body.reserve(len);
            unsigned long deadline = millis() + 300;
            while (static_cast<int>(body.length()) < len && millis() < deadline) {
                if (client.available()) body += static_cast<char>(client.read());
                else delay(1);
            }
        }
    }

    body.trim();
    Serial.printf("[Web] POST /wifi  %u байт\r\n", body.length());

    if (body.isEmpty()) {
        server_.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    // Ожидаем JSON вида: {"wifi":{"ssid":"...","password":"..."}}
    if (config.from_json(body)) {
        server_.send(200, "application/json", "{\"ok\":true}");
    } else {
        server_.send(400, "application/json", "{\"error\":\"invalid json\"}");
    }
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
