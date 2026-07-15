#include "WebManager.h"

#include "ConfigManager.h"
#include "AlertManager.h"
#include "CanBusManager.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>

// Вернуть метрику CAN с учётом таймаута устаревания из конфига
static float web_can_value(float value, uint32_t ts)
{
    if (ts == 0) return 0.0f;

    uint32_t stale_ms = static_cast<uint32_t>(config.get("system", "stale_ms"));
    return (millis() - ts <= stale_ms) ? value : 0.0f;
}

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
  /* ── Вкладки ──────────────────────────────────────── */
  .tabs {
    max-width: 960px;
    margin: 0 auto 10px;
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .tab-btn {
    min-width: 0;
    background: var(--card);
    color: var(--muted);
    border: 1px solid var(--border);
  }
  .tab-btn.active {
    background: var(--accent);
    color: #000;
  }
  .tab-panel { display: none; }
  .tab-panel.active { display: block; }
  /* ── Монитор ──────────────────────────────────────── */
  .monitor-wrap {
    max-width: 960px;
    margin: 0 auto;
  }
  .monitor-status {
    display: flex;
    justify-content: space-between;
    gap: 10px;
    color: var(--muted);
    font-size: 0.72rem;
    margin-bottom: 8px;
    flex-wrap: wrap;
  }
  .monitor-screen {
    background: #050505;
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 10px;
    box-shadow: inset 0 0 40px rgba(201,168,76,0.08);
  }
  .monitor-title {
    text-align: center;
    color: #fff;
    letter-spacing: 2px;
    font-weight: 700;
    margin-bottom: 0;
    font-size: 1rem;
  }
  .monitor-subtitle {
    text-align: center;
    color: var(--accent);
    letter-spacing: 3px;
    font-size: 0.68rem;
    margin-bottom: 8px;
  }
  .monitor-section {
    border-top: 1px solid #5AEB;
    padding-top: 6px;
    margin-top: 8px;
  }
  .monitor-section h3 {
    color: #8ec6c6;
    text-align: center;
    font-size: 0.58rem;
    letter-spacing: 1.5px;
    margin-bottom: 6px;
    font-weight: 600;
  }
  .monitor-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 6px;
  }
  .metric-tile {
    background: #0d0d0d;
    border: 1px solid #1e1e1e;
    border-radius: 8px;
    padding: 7px 4px;
    text-align: center;
    min-height: 58px;
  }
  .metric-label {
    color: #9CD3;
    font-size: 0.52rem;
    letter-spacing: 0.6px;
    margin-bottom: 3px;
    white-space: nowrap;
  }
  .metric-value {
    font-size: clamp(1.05rem, 6.5vw, 1.55rem);
    font-weight: 800;
    font-family: 'Segoe UI', Arial, sans-serif;
    line-height: 1;
  }
  .metric-unit {
    color: var(--muted);
    font-size: 0.52rem;
    margin-top: 2px;
  }
  .metric-blue { color: var(--blue); }
  .metric-green { color: var(--green); }
  .metric-red { color: var(--red); }
  .metric-yellow { color: #ffc107; }
  .monitor-alert {
    display: none;
    margin-bottom: 8px;
    padding: 8px;
    border: 1px solid var(--red);
    border-radius: 10px;
    color: var(--red);
    text-align: center;
    font-weight: 700;
    letter-spacing: 1px;
  }
  footer {
    text-align: center;
    color: var(--muted);
    font-size: 0.75rem;
    margin-top: 36px;
    letter-spacing: 1px;
  }
  @media (max-width: 620px) {
    body { padding: 8px; }
    .tabs { grid-template-columns: 1fr; }
    .tab-btn { padding: 8px 10px; }
    .monitor-grid { grid-template-columns: repeat(3, minmax(0, 1fr)); }
  }
</style>
</head>
<body>

<header>
  <h1>&#9670; INFINITI QX50 J55 &#9670;</h1>
  <h2>MONITORING &mdash; Редактор конфигурации</h2>
</header>

<nav class="tabs">
  <button type="button" class="tab-btn active" data-tab="settingsTab">1. Настройки</button>
  <button type="button" class="tab-btn" data-tab="monitorTab">2. Онлайн-монитор</button>
</nav>

<main id="settingsTab" class="tab-panel active">

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

</main>

<main id="monitorTab" class="tab-panel">
  <div class="monitor-wrap">
    <div class="monitor-status">
      <span id="monitorConn">Связь: ожидание</span>
      <span id="monitorUpdated">Обновление: --</span>
    </div>
    <div class="monitor-alert" id="monitorAlert"></div>
    <div class="monitor-screen">
      <div class="monitor-title">INFINITI QX50 J55</div>
      <div class="monitor-subtitle">MONITORING</div>

      <div class="monitor-section">
        <h3>TEMPERATURE, C</h3>
        <div class="monitor-grid">
          <div class="metric-tile"><div class="metric-label">RAD-ANT</div><div class="metric-value" id="mRadiator">--</div><div class="metric-unit">°C</div></div>
          <div class="metric-tile"><div class="metric-label">ENG-ANT</div><div class="metric-value" id="mCoolant">--</div><div class="metric-unit">°C</div></div>
          <div class="metric-tile"><div class="metric-label">ENG-OIL</div><div class="metric-value" id="mOil">--</div><div class="metric-unit">°C</div></div>
        </div>
      </div>

      <div class="monitor-section">
        <h3>ENGINE</h3>
        <div class="monitor-grid">
          <div class="metric-tile"><div class="metric-label">ENG-RPM</div><div class="metric-value" id="mRpm">--</div><div class="metric-unit">об/мин</div></div>
          <div class="metric-tile"><div class="metric-label">OIL-PR-V</div><div class="metric-value" id="mOilPressure">--</div><div class="metric-unit">В</div></div>
          <div class="metric-tile"><div class="metric-label">TURBO-V</div><div class="metric-value" id="mBoost">--</div><div class="metric-unit">В</div></div>
        </div>
      </div>

      <div class="monitor-section">
        <h3>OTHER</h3>
        <div class="monitor-grid">
          <div class="metric-tile"><div class="metric-label">BATTERY-V</div><div class="metric-value" id="mBattery">--</div><div class="metric-unit">В</div></div>
          <div class="metric-tile"><div class="metric-label">RPM-POLL</div><div class="metric-value" id="mPollTime">--</div><div class="metric-unit">с</div></div>
          <div class="metric-tile"><div class="metric-label">CVT-FLD</div><div class="metric-value" id="mTransmission">--</div><div class="metric-unit">°C</div></div>
        </div>
      </div>
    </div>
  </div>
</main>

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

let currentConfig = null;
let monitorTimer = null;

document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.tab).classList.add('active');
    if (btn.dataset.tab === 'monitorTab') startMonitor();
    else stopMonitor();
  });
});

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

function metricClass(state) {
  return 'metric-value metric-' + (state || 'blue');
}

function classifyTemperature(value, section) {
  const cfg = currentConfig && currentConfig[section];
  if (!cfg || value === 0) return 'blue';
  if (value >= cfg.max) return 'red';
  if (value <= cfg.min) return 'blue';
  return 'green';
}

function classifyRpm(value) {
  const cfg = currentConfig && currentConfig.rpm;
  if (!cfg || value <= cfg.green_start) return 'blue';
  if (value >= cfg.red_start) return 'red';
  if (value <= cfg.green_end) return 'green';
  return 'yellow';
}

function classifyOilPressure(value, rpm) {
  const cfg = currentConfig && currentConfig.oil_pressure;
  if (!cfg || value === 0) return 'blue';
  const minValue = rpm < cfg.rpm_threshold ? cfg.min_low : cfg.min_high;
  return value < minValue ? 'red' : 'green';
}

function classifyBoost(value) {
  const cfg = currentConfig && currentConfig.boost;
  if (!cfg || value <= cfg.blue_max) return 'blue';
  if (value >= cfg.green_min) return 'green';
  return 'yellow';
}

function classifyBattery(value) {
  const cfg = currentConfig && currentConfig.battery;
  if (!cfg || value === 0) return 'blue';
  if (value < cfg.red_low || value > cfg.red_high) return 'red';
  if (value >= cfg.green_min && value <= cfg.green_max) return 'green';
  return 'yellow';
}

function classifyPollTime(value, rpm) {
  const cfg = currentConfig && currentConfig.poll_time;
  if (!cfg || value === 0 || rpm === 0) return 'blue';
  if (value <= cfg.green_max) return 'green';
  if (value >= cfg.red_min) return 'red';
  return 'yellow';
}

function setMetric(id, value, digits, state) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = value === 0 ? '0' : Number(value).toFixed(digits);
  el.className = metricClass(state);
}

async function updateMonitor() {
  try {
    const r = await fetch('/metrics', { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const data = await r.json();
    const m = data.metrics || {};

    setMetric('mRadiator', m.radiator_coolant || 0, 0, classifyTemperature(m.radiator_coolant || 0, 'radiator'));
    setMetric('mCoolant', m.engine_coolant || 0, 0, classifyTemperature(m.engine_coolant || 0, 'coolant'));
    setMetric('mOil', m.engine_oil || 0, 0, classifyTemperature(m.engine_oil || 0, 'oil'));
    setMetric('mRpm', m.engine_rpm || 0, 0, classifyRpm(m.engine_rpm || 0));
    setMetric('mOilPressure', m.oil_pressure_volt || 0, 2, classifyOilPressure(m.oil_pressure_volt || 0, m.engine_rpm || 0));
    setMetric('mBoost', m.turbo_boost_volt || 0, 2, classifyBoost(m.turbo_boost_volt || 0));
    setMetric('mBattery', m.battery_voltage || 0, 2, classifyBattery(m.battery_voltage || 0));
    setMetric('mPollTime', m.rpm_poll_time || 0, 2, classifyPollTime(m.rpm_poll_time || 0, m.engine_rpm || 0));
    setMetric('mTransmission', m.cvt_temp || 0, 0, classifyTemperature(m.cvt_temp || 0, 'transmission'));

    const alertBox = document.getElementById('monitorAlert');
    if (data.alert && data.alert.active) {
      alertBox.style.display = 'block';
      alertBox.textContent = data.alert.code + ' — ' + data.alert.description;
    } else {
      alertBox.style.display = 'none';
    }

    document.getElementById('monitorConn').textContent = 'Связь: есть';
    document.getElementById('monitorUpdated').textContent = 'Обновление: ' + new Date().toLocaleTimeString();
  } catch(e) {
    document.getElementById('monitorConn').textContent = 'Связь: ошибка ' + e.message;
  }
}

function startMonitor() {
  updateMonitor();
  if (!monitorTimer) monitorTimer = setInterval(updateMonitor, 500);
}

function stopMonitor() {
  if (!monitorTimer) return;
  clearInterval(monitorTimer);
  monitorTimer = null;
}

async function loadConfig() {
  try {
    const r = await fetch('/config');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const cfg = await r.json();
    currentConfig = cfg;
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
    currentConfig = Object.assign({}, currentConfig || {}, data);
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
    currentConfig = cfg;
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
  if (currentConfig) currentConfig.system = { poll_interval_ms: poll_ms, stale_ms: stale_ms };
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
    server_.on("/metrics",       HTTP_GET,  [this]() { handle_get_metrics(); });

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

void WebManager::handle_get_metrics()
{
    JsonDocument doc;
    JsonObject metrics = doc["metrics"].to<JsonObject>();

    metrics["engine_coolant"]     = web_can_value(can_metrics.engine_coolant, can_metrics.engine_coolant_ts);
    metrics["engine_oil"]         = web_can_value(can_metrics.engine_oil, can_metrics.engine_oil_ts);
    metrics["radiator_coolant"]   = web_can_value(can_metrics.radiator_coolant, can_metrics.radiator_coolant_ts);
    metrics["engine_rpm"]         = web_can_value(can_metrics.engine_rpm, can_metrics.engine_rpm_ts);
    metrics["turbo_boost_volt"]   = web_can_value(can_metrics.turbo_boost_volt, can_metrics.turbo_boost_volt_ts);
    metrics["oil_pressure_volt"]  = web_can_value(can_metrics.oil_pressure_volt, can_metrics.oil_pressure_volt_ts);
    metrics["battery_voltage"]    = web_can_value(can_metrics.battery_voltage, can_metrics.battery_voltage_ts);
    metrics["cvt_temp"]           = web_can_value(can_metrics.cvt_temp, can_metrics.cvt_temp_ts);
    metrics["rpm_poll_time"]      = (can_metrics.rpm_poll_time_ts != 0) ? can_metrics.rpm_poll_time : 0.0f;

    JsonObject alert = doc["alert"].to<JsonObject>();
    alert["active"] = alert_manager.has_active_alert();
    alert["code"] = alert_manager.active_code();
    alert["description"] = alert_manager.active_description();
    alert["has_log"] = alert_manager.log_count() > 0;

    doc["uptime_ms"] = millis();

    String json;
    serializeJson(doc, json);
    server_.send(200, "application/json", json);
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
