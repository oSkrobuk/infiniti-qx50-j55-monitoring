#include "WebManager.h"

#include "ConfigManager.h"
#include "AlertManager.h"
#include "CanBusManager.h"
#include "BuildInfo.h"
#include "DiagnosticMode.h"
#include "DiagnosticSelection.h"
#include "OtaSlots.h"
#include "ObdPidCatalog.h"
#include "ResetHistory.h"
#include "Version.h"
#include <Update.h>
#include <WiFi.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>

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
  .field input[type="password"],
  .field select {
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
  .field input[type="password"]:focus,
  .field select:focus { border-color: var(--accent); }
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
  .hint {
    display: block;
    font-size: 0.68rem;
    color: var(--muted);
    margin-top: 6px;
    line-height: 1.35;
    white-space: normal;
  }
  .brightness-box {
    margin-top: 10px;
    padding: 12px;
    background: #111;
    border: 1px solid var(--border);
    border-radius: 8px;
  }
  .brightness-box .field {
    max-width: 260px;
  }
  .brightness-box .field label {
    white-space: normal;
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
  /* ── Проверка обновлений ─────────────────────────── */
  .upd-card { margin-bottom: 16px; }
  .upd-row {
    display: flex;
    gap: 12px;
    align-items: center;
    flex-wrap: wrap;
    margin-bottom: 12px;
  }
  .upd-cur {
    flex: 1;
    min-width: 180px;
    font-size: 0.85rem;
    color: var(--muted);
  }
  .upd-cur b { color: var(--text); }
  .upd-hint {
    font-size: 0.72rem;
    color: var(--muted);
    line-height: 1.4;
    margin-top: 4px;
  }
  .btn-upd {
    flex: 0 0 auto;
    min-width: 0;
    padding: 10px 18px;
    font-size: 0.85rem;
    background: var(--border);
    color: var(--accent);
    border: 1px solid var(--accent);
    border-radius: 8px;
  }
  .btn-upd:hover { opacity: 0.8; }
  .btn-upd-install {
    background: #1565c0;
    color: #fff;
    border-color: #1565c0;
  }
  .upd-box {
    display: none;
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 12px 14px;
    font-size: 0.85rem;
    line-height: 1.5;
    background: var(--bg);
  }
  .upd-box.show { display: block; }
  .upd-box.new  { border-color: var(--accent); }
  .upd-box.ok   { border-color: var(--green); }
  .upd-box.err  { border-color: var(--red); }
  .upd-title { font-weight: 600; margin-bottom: 6px; }
  .upd-box.new .upd-title { color: var(--accent); }
  .upd-box.ok  .upd-title { color: var(--green); }
  .upd-box.err .upd-title { color: var(--red); }
  .upd-notes {
    margin-top: 8px;
    padding-top: 8px;
    border-top: 1px solid var(--border);
    max-height: 200px;
    overflow: auto;
    white-space: pre-wrap;
    font-size: 0.78rem;
    color: var(--muted);
  }
  .upd-links {
    display: flex;
    gap: 14px;
    align-items: center;
    flex-wrap: wrap;
    margin-top: 10px;
  }
  .upd-links a { color: #64b5f6; font-size: 0.8rem; font-weight: 600; }
  /* Ссылка «Все релизы» живет в примечании — там штатный цвет браузера
     на темном фоне почти не читается, поэтому задаем явно */
  .wifi-note a { color: #64b5f6; font-weight: 600; }
  /* ── Слоты прошивки ──────────────────────────────── */
  .slot-list { display: flex; flex-direction: column; gap: 10px; }
  .slot {
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px 12px;
    background: var(--bg);
  }
  .slot.run { border-color: var(--green); }
  .slot-head {
    display: flex;
    gap: 8px;
    align-items: center;
    flex-wrap: wrap;
  }
  .slot-name { font-size: 0.9rem; font-weight: 600; letter-spacing: 1px; }
  .slot-badge {
    font-size: 0.68rem;
    letter-spacing: 0.5px;
    padding: 2px 8px;
    border: 1px solid var(--border);
    border-radius: 10px;
    color: var(--muted);
  }
  .slot-badge.run  { border-color: var(--green);  color: var(--green); }
  .slot-badge.boot { border-color: var(--accent); color: var(--accent); }
  .btn-slot {
    flex: 0 0 auto;
    min-width: 0;
    margin-left: auto;
    padding: 7px 14px;
    font-size: 0.75rem;
    background: var(--border);
    color: var(--accent);
    border: 1px solid var(--accent);
    border-radius: 6px;
  }
  .btn-slot:hover { opacity: 0.8; }
  .slot-ver { font-size: 0.85rem; color: var(--muted); margin-top: 7px; }
  .slot-ver b { color: var(--text); }
  .slot-meta { font-size: 0.72rem; color: var(--muted); margin-top: 3px; }
  /* ── Окно «доступна новая прошивка» ──────────────── */
  .modal {
    display: none;
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0, 0, 0, 0.72);
    z-index: 200;
    align-items: center;
    justify-content: center;
    padding: 20px;
  }
  .modal.show { display: flex; }
  .modal-card {
    background: var(--card);
    border: 1px solid var(--accent);
    border-radius: 12px;
    padding: 20px 22px;
    width: 100%;
    max-width: 420px;
  }
  .modal-title {
    font-size: 0.95rem;
    letter-spacing: 1px;
    color: var(--accent);
    font-weight: 600;
    margin-bottom: 10px;
  }
  .modal-text {
    font-size: 0.85rem;
    line-height: 1.55;
    color: var(--text);
    margin-bottom: 16px;
  }
  .modal-text b { color: var(--accent); }
  .modal-actions { display: flex; gap: 10px; flex-wrap: wrap; }
  .btn-modal-go    { background: var(--accent); color: #000; }
  .btn-modal-later { background: var(--border); color: var(--muted); border: 1px solid #444; }
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
  <a class="nav-link" href="/obd">OBD-II PID &rarr;</a>
  <a class="nav-link" href="/health">&#128295; Работоспособность устройства &rarr;</a>
</header>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 1. НАСТРОЙКА WIFI                                                       -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectWifi">
  <summary>
    <span class="sect-title">&#128267; 1. Настройка WiFi</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">
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
    </div>
    <div class="actions">
      <button type="button" class="btn-default" onclick="resetWifiCard()">&#8635; Сбросить</button>
      <button type="submit" class="btn-save" id="btnSaveWifi">&#10003; Сохранить WiFi</button>
    </div>
    </form>
  </div>
</details>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 2. СИСТЕМНЫЕ ПАРАМЕТРЫ                                                  -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectSystem">
  <summary>
    <span class="sect-title">&#9881; 2. Системные параметры</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">
    <form id="systemForm">
    <div class="grid">
      <div class="card">
        <div class="card-hdr"><div class="card-title">&#9201; Система &mdash; Параметры</div><button type="button" class="btn-card-default" onclick="resetCardFields('system')">&#8635; Сброс</button></div>
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
        <div class="brightness-box">
          <div class="field">
            <label>Яркость подсветки</label>
            <select name="system_brightness_percent" required>
              <option value="10">10%</option><option value="20">20%</option>
              <option value="30">30%</option><option value="40">40%</option>
              <option value="50">50%</option><option value="60">60%</option>
              <option value="70">70%</option><option value="80">80%</option>
              <option value="90">90%</option><option value="100" selected>100%</option>
            </select>
            <span class="hint">Регулировка работает только на дисплеях с входом BLK</span>
          </div>
        </div>
      </div>
    </div>

    <div class="actions">
      <button type="button" class="btn-default" onclick="resetCardFields('system')">&#8635; Сбросить все</button>
      <button type="submit" class="btn-save" id="btnSaveSystem">&#10003; Сохранить параметры</button>
    </div>
    </form>

  </div>
</details>

<!-- ═══════════════════════════════════════════════════════════════════════ -->
<!-- 3. ИСТОРИЯ СРАБАТЫВАНИЙ                                                 -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectAlerts">
  <summary>
    <span class="sect-title">&#128680; 3. История срабатываний</span>
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
<!-- 4. КОНФИГУРАЦИЯ ПРОВЕРОК                                                -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectChecks">
  <summary>
    <span class="sect-title">&#9888; 4. Конфигурация проверок</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">
    <div class="grid">
      <div class="card">
        <div class="card-hdr">
          <div class="card-title">&#9201; Тайминги алертов</div>
          <button type="button" class="btn-card-default" onclick="resetCheckTiming()">&#8635; Сброс</button>
        </div>
        <div style="font-size:0.75rem;color:var(--muted);margin-bottom:10px;line-height:1.4">
          Общие для всех проверок. Подтверждение отсекает кратковременные выбросы показаний:
          алерт поднимется, только если условие продержится указанное время. Повтор задает,
          как часто напоминать о сохраняющейся неисправности.
        </div>
        <div class="row2">
          <div class="field">
            <label>Подтверждение, с</label>
            <input class="f-target" type="number" step="0.1" min="0" max="60" id="check_confirm_s">
          </div>
          <div class="field">
            <label>Повтор алерта, с</label>
            <input class="f-target" type="number" step="1" min="1" max="3600"
                   id="check_retrigger_s" oninput="updateRetriggerLabels()">
          </div>
        </div>
      </div>
    </div>
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
<!-- 5. КОНФИГУРАЦИЯ МЕТРИК                                                  -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectMetrics">
  <summary>
    <span class="sect-title">&#128202; 5. Конфигурация метрик</span>
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
        <div class="card-hdr"><div class="card-title">&#128167; EOP &mdash; Датчик давления масла</div><button type="button" class="btn-card-default" onclick="resetCardFields('oil_pressure')">&#8635; Сброс</button></div>
        <div class="row3">
          <div class="field">
            <label>Порог RPM</label>
            <input class="f-target" type="number" step="100" name="oil_pressure_rpm_threshold" required>
          </div>
          <div class="field">
            <label>Мин &lt;порога, В</label>
            <input class="f-min" type="number" step="0.01" name="oil_pressure_min_low" required>
          </div>
          <div class="field">
            <label>Мин &ge;порога, В</label>
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
        <div class="card-hdr"><div class="card-title">&#9201; RPM-POLL &mdash; Период обновления RPM</div><button type="button" class="btn-card-default" onclick="resetCardFields('poll_time')">&#8635; Сброс</button></div>
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
<!-- 6. ОБНОВЛЕНИЕ ПРОШИВКИ                                                  -->
<!-- ═══════════════════════════════════════════════════════════════════════ -->
<details class="sect" id="sectOta">
  <summary>
    <span class="sect-title">&#8593; 6. Обновление прошивки</span>
    <span class="sect-chevron">&#9660;</span>
  </summary>
  <div class="sect-body">
    <!-- Проверка новой версии на GitHub — запрос уходит из браузера телефона -->
    <div class="card upd-card">
      <div class="card-title">&#8635; Проверка обновлений</div>
      <div class="upd-row">
        <div class="upd-cur">
          Текущая версия: <b id="updCur">&mdash;</b>
          <div class="upd-hint" id="updMeta"></div>
        </div>
        <button type="button" class="btn-upd" id="btnUpdCheck">&#8635; Проверить обновления</button>
      </div>
      <div class="upd-box" id="updBox"></div>
      <p class="wifi-note">
        Проверка идет из браузера прямо в GitHub — устройству интернет не нужен.
        Включите на телефоне мобильные данные и разрешите ему оставаться в сети без интернета.
        <a href="https://github.com/oSkrobuk/infiniti-qx50-j55-monitoring/releases" target="_blank" rel="noopener">Все релизы</a>
      </p>
    </div>

    <!-- Что лежит в обоих слотах OTA и переключение загрузочного -->
    <div class="card">
      <div class="card-title">&#8646; Слоты прошивки</div>
      <div class="slot-list" id="slotList">
        <div class="upd-hint">Читаю слоты...</div>
      </div>
      <p class="wifi-note">
        Обновление всегда пишется в свободный слот, а прежняя прошивка остается во втором.
        Переключение меняет только то, откуда устройство загрузится: настройки, журнал алертов
        и файловая система общие и не затрагиваются.
      </p>
    </div>

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

<!-- Окно о новой прошивке — показывается только после успешной автопроверки -->
<div class="modal" id="updModal">
  <div class="modal-card">
    <div class="modal-title">&#8593; Доступна новая прошивка</div>
    <div class="modal-text" id="updModalText"></div>
    <div class="modal-actions">
      <button type="button" class="btn-modal-go" id="btnUpdModalGo">Перейти к обновлению</button>
      <button type="button" class="btn-modal-later" id="btnUpdModalLater">Позже</button>
    </div>
  </div>
</div>

<div class="toast" id="toast"></div>

<footer><a href="https://github.com/oSkrobuk/infiniti-qx50-j55-monitoring" target="_blank" rel="noopener" style="color:var(--accent);text-decoration:none">Infiniti QX50 J55 Monitoring</a> &mdash; ESP32</footer>

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
    const bp = document.querySelector('[name="system_brightness_percent"]');
    if (pi) pi.value = cfg.system.poll_interval_ms;
    if (sm) sm.value = cfg.system.stale_ms;
    if (bp) bp.value = cfg.system.brightness_percent;
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
    return 'EOP: минимальное напряжение должно быть больше 0';
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

// ── WiFi и системные параметры ─────────────────────────────────────────────

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

  const btn = document.getElementById('btnSaveWifi');
  btn.disabled = true;

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

  showToast('✓ WiFi сохранен. Перезагрузка...', 'ok');
  setTimeout(async () => {
    try { await fetch('/restart', { method: 'POST' }); } catch(_) {}
  }, 1200);
});

document.getElementById('systemForm').addEventListener('submit', async (e) => {
  e.preventDefault();

  const pollEl = document.querySelector('[name="system_poll_interval_ms"]');
  const staleEl = document.querySelector('[name="system_stale_ms"]');
  const brightnessEl = document.querySelector('[name="system_brightness_percent"]');
  const poll_ms = parseFloat(pollEl ? pollEl.value : 30);
  const stale_ms = parseFloat(staleEl ? staleEl.value : 1000);
  const brightness_percent = parseFloat(brightnessEl ? brightnessEl.value : 100);

  if (poll_ms < 10) { showToast('⚠ Интервал опроса не может быть меньше 10 мс', 'err'); return; }
  if (stale_ms < 100) { showToast('⚠ Порог устаревания не может быть меньше 100 мс', 'err'); return; }
  if (poll_ms >= stale_ms) {
    showToast('⚠ Интервал опроса должен быть меньше порога устаревания', 'err');
    return;
  }
  if (brightness_percent < 10 || brightness_percent > 100 || brightness_percent % 10 !== 0) {
    showToast('⚠ Яркость должна быть от 10 до 100% с шагом 10%', 'err');
    return;
  }

  const btn = document.getElementById('btnSaveSystem');
  btn.disabled = true;
  try {
    const sysPayload = {
      system: {
        poll_interval_ms: poll_ms,
        stale_ms: stale_ms,
        brightness_percent: brightness_percent
      }
    };
    const r = await fetch('/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(sysPayload)
    });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    showToast('✓ Системные параметры сохранены', 'ok');
  } catch(e) {
    showToast('Ошибка сохранения системных параметров: ' + e.message, 'err');
  } finally {
    btn.disabled = false;
  }
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

// Тайминги хранятся в мс, а в поля вводятся секунды
const CHECK_TIMING_DEFAULTS = { confirm_ms: 1000, retrigger_ms: 15000 };

// Подпись режима повтора: интервал берется из поля, а не из константы
function retriggerLabelText() {
  const el = document.getElementById('check_retrigger_s');
  const sec = el && parseFloat(el.value) > 0
    ? parseFloat(el.value)
    : CHECK_TIMING_DEFAULTS.retrigger_ms / 1000;
  return `При постоянной ошибке повтор раз в ${sec} сек`;
}

// Переписывает подписи всех карточек — вызывается при правке поля повтора
function updateRetriggerLabels() {
  CHECK_DEFS.forEach(def => {
    const chk = document.getElementById(`check_${def.code}_enabled`);
    const lbl = document.getElementById(`check_${def.code}_label`);
    if (chk && lbl) {
      lbl.textContent = chk.checked ? retriggerLabelText() : 'Однократно, пока код в журнале';
    }
  });
}

function renderChecks(cfg) {
  const grid = document.getElementById('checksGrid');
  grid.innerHTML = '';

  // Тайминги заполняем первыми: подписи карточек берут интервал из этих полей
  const timing = cfg.timing || {};
  const confirmMs = timing.confirm_ms !== undefined
    ? timing.confirm_ms : CHECK_TIMING_DEFAULTS.confirm_ms;
  const retriggerMs = timing.retrigger_ms !== undefined
    ? timing.retrigger_ms : CHECK_TIMING_DEFAULTS.retrigger_ms;
  document.getElementById('check_confirm_s').value   = confirmMs / 1000;
  document.getElementById('check_retrigger_s').value = retriggerMs / 1000;

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
            ${checkCfg.enabled ? retriggerLabelText() : 'Однократно, пока код в журнале'}
          </span>
      </div>
      ${paramsHtml}`;
    grid.appendChild(card);

    const chk = document.getElementById(`check_${def.code}_enabled`);
    const lbl = document.getElementById(`check_${def.code}_label`);
    chk.addEventListener('change', () => {
      lbl.textContent = chk.checked ? retriggerLabelText() : 'Однократно, пока код в журнале';
    });
  });
}

function readChecks() {
  const result = {
    timing: {
      confirm_ms:   Math.round((parseFloat(document.getElementById('check_confirm_s').value) || 0) * 1000),
      retrigger_ms: Math.round((parseFloat(document.getElementById('check_retrigger_s').value) || 0) * 1000)
    }
  };
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

// Отправка прошивки в POST /update — общий код ручной загрузки и автоустановки.
// file — File из формы или Blob, скачанный с GitHub; btn разблокируется при ошибке
function sendFirmware(file, name, btn) {
  const formData = new FormData();
  formData.append('firmware', file, name);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');

  if (btn) btn.disabled = true;
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
      if (btn) btn.disabled = false;
    }
  });

  xhr.addEventListener('error', () => {
    otaBar.style.background = 'var(--red)';
    otaStatus.textContent = '✗ Сетевая ошибка';
    if (btn) btn.disabled = false;
  });

  xhr.send(formData);
}

btnOta.addEventListener('click', () => {
  if (!otaFile.files.length) return;
  if (!confirm('Загрузить новую прошивку?\nУстройство перезагрузится после прошивки.')) return;

  sendFirmware(otaFile.files[0], otaFile.files[0].name, btnOta);
});

// ── Слоты прошивки ─────────────────────────────────────────────────────────
//
// Список читается при раскрытии раздела: версию соседнего слота устройство
// ищет прямо в его образе, и на это уходит доля секунды

const slotList = document.getElementById('slotList');

// Последний ответ /slots — из него берется версия для подтверждения
let slotsCache = [];

function slotKB(bytes) {
  return Math.round(bytes / 1024) + ' KB';
}

function renderSlots(list) {
  slotsCache = list;

  if (!list.length) {
    slotList.innerHTML = '<div class="upd-hint">Слоты OTA не найдены</div>';
    return;
  }

  slotList.innerHTML = list.map(s => {
    let badges = '';
    if (s.running) badges += '<span class="slot-badge run">работает сейчас</span>';
    if (s.boot) {
      badges += '<span class="slot-badge boot">' +
        (s.running ? 'загрузочный' : 'загрузится после перезагрузки') + '</span>';
    }

    let ver;
    if (!s.valid)       ver = 'Слот пуст';
    else if (s.version) ver = 'Версия <b>' + updEsc(s.version) + '</b>';
    else                ver = 'Версия <b>неизвестна</b> — прошивка собрана до появления этой страницы';

    const meta = [];
    if (s.env)   meta.push(updEsc(s.env));
    if (s.build) meta.push('сборка ' + updEsc(s.build));
    if (s.used)  meta.push(slotKB(s.used) + ' из ' + slotKB(s.size));

    const btn = (s.valid && !s.boot)
      ? '<button type="button" class="btn-slot" data-slot="' + updEsc(s.label) +
        '">&#8646; Загрузиться отсюда</button>'
      : '';

    return '<div class="slot' + (s.running ? ' run' : '') + '">' +
      '<div class="slot-head"><span class="slot-name">' + updEsc(s.label) + '</span>' +
      badges + btn + '</div>' +
      '<div class="slot-ver">' + ver + '</div>' +
      (meta.length ? '<div class="slot-meta">' + meta.join(' · ') + '</div>' : '') +
      '</div>';
  }).join('');

  slotList.querySelectorAll('.btn-slot').forEach(b => {
    b.addEventListener('click', () => switchSlot(b.dataset.slot));
  });
}

async function loadSlots() {
  try {
    const r = await fetch('/slots', { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    renderSlots(d.slots || []);
  } catch(e) {
    slotList.innerHTML = '<div class="upd-hint">Не удалось прочитать слоты: ' + updEsc(e.message) + '</div>';
  }
}

// Переключение меняет только запись otadata — прошивка из выбранного слота
// начнет работать после перезагрузки
async function switchSlot(label) {
  const s   = slotsCache.find(x => x.label === label) || {};
  const ver = s.version ? ('версия ' + s.version) : 'версия неизвестна';

  if (!confirm('Загружаться из слота ' + label + ' (' + ver + ')?\nУстройство перезагрузится.')) return;

  try {
    const r = await fetch('/boot-slot?slot=' + encodeURIComponent(label), { method: 'POST' });
    const d = await r.json().catch(() => ({}));
    if (!r.ok) throw new Error(d.error || ('HTTP ' + r.status));
  } catch(e) {
    showToast('Ошибка: ' + e.message, 'err');
    return;
  }

  showToast('✓ Слот переключен. Перезагрузка...', 'ok');
  setTimeout(async () => {
    try { await fetch('/restart', { method: 'POST' }); } catch(_) {}
  }, 1200);
}

document.getElementById('sectOta').addEventListener('toggle', function() {
  if (this.open) loadSlots();
});

// ── Проверка обновлений ────────────────────────────────────────────────────
//
// Устройство работает точкой доступа и в интернет не выходит, поэтому релизы
// спрашивает браузер телефона: запрос на 192.168.4.1 идет по WiFi, запрос на
// api.github.com — по мобильной сети

const UPD_REPO = 'oSkrobuk/infiniti-qx50-j55-monitoring';
const UPD_PAGE = 'https://github.com/' + UPD_REPO + '/releases';
const UPD_API  = 'https://api.github.com/repos/' + UPD_REPO + '/releases/latest';

// Откуда качается прошивка для автоустановки.
//
// Не из ассетов релиза: их GitHub отдает с release-assets.githubusercontent.com
// без заголовка Access-Control-Allow-Origin, и браузер не пускает такой ответ
// в JS — запрос падает еще до чтения тела. Поэтому CI на каждом теге кладет
// OTA-образы в ветку firmware, а ее файлы raw.githubusercontent.com отдает
// с CORS. Ссылки на ассеты остаются: они открываются обычным скачиванием,
// которому CORS не нужен
const UPD_RAW = 'https://raw.githubusercontent.com/' + UPD_REPO + '/firmware/';

// Таймаут запроса к GitHub, мс. В сети без интернета DNS не отвечает, и без
// AbortController запрос висел бы десятками секунд
const UPD_TIMEOUT_MS = 5000;

// Таймаут скачивания OTA-образа, мс — файл около мегабайта
const UPD_DOWNLOAD_TIMEOUT_MS = 120000;

// Не чаще одной автопроверки в 5 минут: страницу открывают и обновляют часто,
// а у GitHub без авторизации всего 60 запросов в час на адрес
const UPD_AUTO_INTERVAL_MS = 300000;

// Ключ в localStorage со временем последней проверки. Именно localStorage, а не
// cookie: cookie браузер прикреплял бы к каждому запросу к устройству, включая
// опрос /metrics раз в полсекунды
const UPD_LAST_KEY = 'qx50_upd_last_check';

const updCur      = document.getElementById('updCur');
const updMeta     = document.getElementById('updMeta');
const updBox      = document.getElementById('updBox');
const btnUpdCheck = document.getElementById('btnUpdCheck');

// Версия и окружение прошивки на устройстве — заполняются из GET /version
let fwVersion = '';
let fwEnv     = '';

// Запас на случай, когда localStorage недоступен (приватный режим): отметка
// живет хотя бы в пределах одной загрузки страницы
let updLastFallback = 0;

function updLastCheck() {
  try {
    return parseInt(localStorage.getItem(UPD_LAST_KEY), 10) || 0;
  } catch(e) {
    return updLastFallback;
  }
}

// Вызывается, когда GitHub ответил — неважно, что именно: отказ по лимиту
// тоже считается проверкой, повторять его каждую перезагрузку незачем
function updMarkChecked() {
  updLastFallback = Date.now();
  try {
    localStorage.setItem(UPD_LAST_KEY, String(updLastFallback));
  } catch(e) {
    // Хранилище недоступно — остаемся с отметкой в памяти
  }
}

// Экранирование текста из ответа GitHub (описание релиза — произвольный markdown)
function updEsc(s) {
  const d = document.createElement('div');
  d.textContent = (s === undefined || s === null) ? '' : String(s);
  return d.innerHTML;
}

// Разбор версии YYYY.M.Z в три числа, префикс v необязателен
function updParseVer(s) {
  const parts = String(s || '').trim().replace(/^v/i, '').split('.');
  return [0, 1, 2].map(i => {
    const n = parseInt(parts[i], 10);
    return isNaN(n) ? 0 : n;
  });
}

// -1 если a старше b, 0 если совпадают, +1 если a новее b
function updCmpVer(a, b) {
  const x = updParseVer(a);
  const y = updParseVer(b);
  for (let i = 0; i < 3; i++) {
    if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  }
  return 0;
}

// Ссылка на страницу релизов — фолбэк, который работает и без fetch
function updPageLink(text) {
  return '<div class="upd-links"><a href="' + UPD_PAGE + '" target="_blank" rel="noopener">' +
    (text || 'Открыть страницу релизов') + '</a></div>';
}

function updShow(kind, title, body) {
  updBox.className = 'upd-box show ' + kind;
  updBox.innerHTML = '<div class="upd-title">' + title + '</div>' + (body || '');
}

// Версия прошивки на устройстве
async function loadVersion() {
  try {
    const r = await fetch('/version', { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const v = await r.json();
    fwVersion = v.version || '';
    fwEnv     = v.env || '';
    updCur.textContent  = fwVersion || 'неизвестна';
    updMeta.textContent = 'сборка ' + (v.build || '?') + ' · слот ' + (v.slot || '?') +
      (fwEnv ? ' · ' + fwEnv : '');
  } catch(e) {
    updCur.textContent  = 'неизвестна';
    updMeta.textContent = '';
  }
}

// Выбрать OTA-образ для окружения сборки
function updFirmwareName() {
  if (/^esp32s3-wt32(-mock)?$/.test(fwEnv)) return 'firmware_s3.bin';
  if (/^esp32(-mock)?$/.test(fwEnv)) return 'firmware.bin';
  return '';
}

// Скачать OTA-образ версии tag из ветки firmware и отдать его в POST /update.
// Версия входит в путь, поэтому приехать может только прошивка этого релиза.
// При любой ошибке остается ручной сценарий — ссылка на ассет и форма ниже
async function updDownloadAndInstall(tag) {
  const btn = document.getElementById('btnUpdInstall');
  const st  = document.getElementById('updInstallStatus');
  const firmwareName = updFirmwareName();

  if (!firmwareName) return;

  if (!confirm('Скачать версию ' + tag + ' и прошить устройство?\nПосле записи устройство перезагрузится.')) return;

  btn.disabled   = true;
  st.textContent = 'Скачиваю ' + firmwareName + '...';

  const ctl   = new AbortController();
  const timer = setTimeout(() => ctl.abort(), UPD_DOWNLOAD_TIMEOUT_MS);
  let blob = null;

  try {
    const r = await fetch(UPD_RAW + encodeURIComponent(tag) + '/' + firmwareName,
      { cache: 'no-store', signal: ctl.signal });
    // 404 — CI еще не выложил сборку этой версии в ветку firmware
    if (r.status === 404) throw new Error('сборки ' + tag + ' нет в ветке firmware');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    blob = await r.blob();
    if (!blob.size) throw new Error('пустой файл');
  } catch(e) {
    st.textContent = 'Скачать не удалось (' + (e.name === 'AbortError' ? 'долго нет ответа' : e.message) +
      '). Нажмите «Скачать ' + firmwareName + '» выше, затем выберите файл в форме «OTA — Загрузка .bin» ниже.';
    btn.disabled = false;
    return;
  } finally {
    clearTimeout(timer);
  }

  st.textContent = 'Скачано ' + Math.round(blob.size / 1024) + ' KB, прошиваю — прогресс ниже';
  sendFirmware(blob, firmwareName, btn);
}

// Отрисовать ответ GitHub о последнем релизе
function updRenderRelease(rel) {
  const tag  = String(rel.tag_name || '').replace(/^v/i, '');
  const page = rel.html_url || UPD_PAGE;
  const cmp  = updCmpVer(tag, fwVersion);

  if (cmp === 0) {
    updShow('ok', '✓ Установлена последняя версия ' + updEsc(tag), updPageLink('Страница релиза'));
    return;
  }
  if (cmp < 0) {
    updShow('ok', 'На устройстве версия новее опубликованной',
      '<div>Последний релиз на GitHub — ' + updEsc(tag) + ', на устройстве ' + updEsc(fwVersion || '?') + '.</div>' +
      updPageLink('Страница релизов'));
    return;
  }

  const firmwareName = updFirmwareName();
  const asset = (rel.assets || []).find(a => a.name === firmwareName);
  const date  = rel.published_at ? String(rel.published_at).slice(0, 10) : '';
  const notes = rel.body ? '<div class="upd-notes">' + updEsc(rel.body) + '</div>' : '';

  let links = '<div class="upd-links"><a href="' + page + '" target="_blank" rel="noopener">Страница релиза</a>';
  if (asset) {
    links += '<a href="' + asset.browser_download_url + '" target="_blank" rel="noopener">Скачать ' +
      updEsc(firmwareName) + '</a>';
  }
  links += '</div>';

  // Автоустановка доступна для известных окружений, чьи OTA-образы публикует CI
  let install = '';
  if (firmwareName) {
    install = '<div class="upd-links">' +
      '<button type="button" class="btn-upd btn-upd-install" id="btnUpdInstall">&#8595; Скачать и установить</button>' +
      '<span class="upd-hint" id="updInstallStatus"></span></div>';
  } else {
    install = '<div class="upd-hint">&#9888; Неизвестная платформа (' + updEsc(fwEnv || '?') +
      '), выберите совместимый .bin вручную.</div>';
  }

  updShow('new', 'Доступна версия ' + updEsc(tag) + ' (у вас ' + updEsc(fwVersion || '?') + ')',
    (date ? '<div>Опубликован ' + updEsc(date) + '</div>' : '') + notes + links + install);

  const btnInstall = document.getElementById('btnUpdInstall');
  if (btnInstall) {
    btnInstall.addEventListener('click', () => updDownloadAndInstall(tag));
  }
}

// Проверка по кнопке — идет всегда, интервал автопроверки ее не ограничивает
async function checkUpdates() {
  const label = btnUpdCheck.innerHTML;
  btnUpdCheck.disabled  = true;
  btnUpdCheck.textContent = 'Проверяю...';

  const ctl   = new AbortController();
  const timer = setTimeout(() => ctl.abort(), UPD_TIMEOUT_MS);

  try {
    const r = await fetch(UPD_API, { cache: 'no-store', signal: ctl.signal });
    updMarkChecked();
    if (r.status === 404) {
      updShow('err', 'Релизы еще не опубликованы', updPageLink());
    } else if (r.status === 403) {
      updShow('err', 'Превышен лимит запросов к GitHub, попробуйте позже',
        '<div class="upd-hint">Без авторизации GitHub отдает 60 запросов в час на один адрес.</div>' +
        updPageLink());
    } else if (!r.ok) {
      updShow('err', 'GitHub ответил HTTP ' + r.status, updPageLink());
    } else {
      updRenderRelease(await r.json());
    }
  } catch(e) {
    updShow('err', 'Нет доступа к интернету',
      '<div>Включите мобильные данные и разрешите телефону оставаться в сети без интернета.</div>' +
      updPageLink('Проверить вручную'));
  } finally {
    clearTimeout(timer);
    btnUpdCheck.disabled  = false;
    btnUpdCheck.innerHTML = label;
  }
}

btnUpdCheck.addEventListener('click', checkUpdates);

// ── Окно «доступна новая прошивка» ─────────────────────────────────────────

const updModal     = document.getElementById('updModal');
const updModalText = document.getElementById('updModalText');

function updModalHide() {
  updModal.classList.remove('show');
}

function updModalShow(tag) {
  updModalText.innerHTML = 'На устройстве версия <b>' + updEsc(fwVersion || '?') +
    '</b>, а на GitHub опубликована <b>' + updEsc(tag) + '</b>.<br>' +
    'Описание релиза и загрузка — в разделе «Обновление прошивки».';
  updModal.classList.add('show');
}

document.getElementById('btnUpdModalLater').addEventListener('click', updModalHide);

document.getElementById('btnUpdModalGo').addEventListener('click', () => {
  updModalHide();
  const sect = document.getElementById('sectOta');
  sect.open = true;
  sect.scrollIntoView({ behavior: 'smooth', block: 'start' });
});

// Окно не должно мешать работе со страницей: закрывается кликом по затемнению
// и клавишей Escape
updModal.addEventListener('click', (e) => {
  if (e.target === updModal) updModalHide();
});

document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape') updModalHide();
});

// Автопроверка при загрузке страницы — тихая: окно и карточка заполняются
// только если GitHub ответил и версия там новее. Любая ошибка игнорируется,
// страница работает как обычно.
//
// Ходит не чаще UPD_AUTO_INTERVAL_MS: перезагрузка страницы в пределах
// интервала нового запроса не делает
async function autoCheckUpdates() {
  if (!fwVersion) return;

  // Отрицательная разница возможна при переводе часов назад — тогда проверяем
  const since = Date.now() - updLastCheck();
  if (since >= 0 && since < UPD_AUTO_INTERVAL_MS) return;

  const ctl   = new AbortController();
  const timer = setTimeout(() => ctl.abort(), UPD_TIMEOUT_MS);

  try {
    const r = await fetch(UPD_API, { cache: 'no-store', signal: ctl.signal });
    updMarkChecked();
    if (!r.ok) return;
    const rel = await r.json();
    const tag = String(rel.tag_name || '').replace(/^v/i, '');
    if (updCmpVer(tag, fwVersion) <= 0) return;
    updRenderRelease(rel);
    updModalShow(tag);
  } catch(e) {
    // Нет интернета или GitHub недоступен — молчим, это штатная ситуация
  } finally {
    clearTimeout(timer);
  }
}

// ── Сброс карточек к значениям по умолчанию ───────────────────────────────

// Значения по умолчанию совпадают с build_defaults() в ConfigManager.cpp
const CARD_DEFAULTS = {
  wifi:         { ssid: 'QX50Monitoring', password: 'infiniti' },
  system:       { poll_interval_ms: 30, stale_ms: 1000, brightness_percent: 100 },
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

// Сбрасывает все карточки проверок и тайминги к дефолтам
function resetAllChecks() {
  resetCheckTiming();
  Object.keys(CHECK_DEFAULTS).forEach(code => resetCheckCard(code));
}

// Сбрасывает карточку таймингов к дефолтам
function resetCheckTiming() {
  document.getElementById('check_confirm_s').value   = CHECK_TIMING_DEFAULTS.confirm_ms / 1000;
  document.getElementById('check_retrigger_s').value = CHECK_TIMING_DEFAULTS.retrigger_ms / 1000;
  updateRetriggerLabels();
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
    if (labelEl) labelEl.textContent = d.enabled ? retriggerLabelText() : 'Однократно, пока код в журнале';
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
// Автопроверке нужна версия с устройства, поэтому она идет через then, а не
// await: загрузка страницы этого не ждет, и обе функции глушат свои ошибки сами
loadVersion().then(autoCheckUpdates);
// Алерты загружаются при открытии секции (toggle event), но при первом открытии
// секция может быть уже открыта — грузим сразу
loadAlerts();
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────
// Страница работоспособности устройства (GET /health) — хранится во флеш (PROGMEM)

static const char HEALTH_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Работоспособность устройства</title>
<style>
  :root {
    --bg:#0d0d0d; --card:#1a1a1a; --border:#333; --text:#eee;
    --muted:#888; --accent:#d4af37; --ok:#4caf50; --warn:#ff9800; --danger:#f44336;
  }
  * { box-sizing:border-box; margin:0; padding:0; }
  body { background:var(--bg); color:var(--text); font-family:Arial,sans-serif; padding:0 16px 32px; }
  header { max-width:960px; margin:0 auto 20px; padding:24px 0 18px; text-align:center; border-bottom:1px solid var(--border); }
  h1 { color:var(--accent); font-size:1.35rem; letter-spacing:2px; text-transform:uppercase; }
  .nav-link { display:inline-block; margin:12px 4px 0; padding:8px 16px; border:1px solid var(--accent); border-radius:8px; color:var(--accent); text-decoration:none; font-size:.8rem; }
  .nav-link:hover { opacity:.75; }
  .status { max-width:960px; margin:0 auto 16px; padding:10px 16px; border:1px solid var(--border); border-radius:8px; color:var(--muted); display:flex; align-items:center; gap:9px; font-size:.8rem; }
  .status-dot { width:9px; height:9px; border-radius:50%; background:var(--warn); }
  .status.online .status-dot { background:var(--ok); }
  .status.offline .status-dot { background:var(--danger); }
  .diagnostics { max-width:960px; margin:0 auto; padding:16px; background:var(--card); border:1px solid var(--border); border-radius:10px; }
  .diag-head { display:flex; justify-content:space-between; gap:12px; margin-bottom:14px; }
  .diag-title { color:var(--accent); font-size:.9rem; letter-spacing:1px; }
  .diag-note,.diag-label,.diag-help,.reset-history-title { color:var(--muted); }
  .diag-note,.reset-history-title { font-size:.72rem; }
  .diag-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(180px,1fr)); gap:10px; }
  .diag-item { padding:12px; background:#111; border:1px solid var(--border); border-radius:8px; }
  .diag-label { font-size:.7rem; margin-bottom:6px; }
  .diag-value { font-size:.9rem; overflow-wrap:anywhere; }
  .diag-value.ok { color:var(--ok); }
  .diag-value.warn { color:var(--warn); }
  .diag-value.danger { color:var(--danger); }
  .diag-help { font-size:.66rem; line-height:1.4; margin-top:6px; }
  .reset-history { margin-top:14px; padding-top:14px; border-top:1px solid var(--border); }
  .reset-history-title { margin-bottom:8px; }
  .reset-history-list { display:flex; flex-wrap:wrap; gap:7px; }
  .reset-history-item { padding:6px 9px; background:#111; border:1px solid var(--border); border-radius:6px; font-size:.72rem; }
  .reset-history-item.danger { border-color:var(--danger); color:var(--danger); }
  footer { text-align:center; color:var(--muted); font-size:.75rem; margin-top:28px; }
  @media (max-width:700px) {
    .diag-grid { grid-template-columns:repeat(2,minmax(0,1fr)); }
    .diag-head { flex-direction:column; gap:4px; }
  }
</style>
</head>
<body>
<header>
  <h1>&#128295; Работоспособность устройства</h1>
  <a class="nav-link" href="/">&larr; К настройкам</a>
  <a class="nav-link" href="/live">&#128202; Онлайн мониторинг</a>
</header>
<div class="status" id="status"><span class="status-dot"></span><span id="statusText">Подключение...</span></div>
<div class="diagnostics">
  <div class="diag-head"><span class="diag-title">Состояние устройства</span><span class="diag-note">с момента последней загрузки</span></div>
  <div class="diag-grid">
    <div class="diag-item"><div class="diag-label">Причина запуска</div><div class="diag-value" id="diagReset">Загрузка...</div></div>
    <div class="diag-item"><div class="diag-label">Состояние CAN</div><div class="diag-value" id="diagTwai">Загрузка...</div></div>
    <div class="diag-item"><div class="diag-label">Принято CAN-кадров</div><div class="diag-value" id="diagCanCount">0</div></div>
    <div class="diag-item"><div class="diag-label">Сбоев Bus-Off</div><div class="diag-value" id="diagBusOff">0</div></div>
    <div class="diag-item"><div class="diag-label">Восстановлений CAN</div><div class="diag-value" id="diagRecovery">0</div></div>
    <div class="diag-item"><div class="diag-label">Попыток recovery / ошибок</div><div class="diag-value" id="diagRecoveryAttempts">0 / 0</div></div>
    <div class="diag-item"><div class="diag-label">Попыток restart / ошибок</div><div class="diag-value" id="diagRestartAttempts">0 / 0</div></div>
    <div class="diag-item"><div class="diag-label">Последняя ошибка recovery</div><div class="diag-value" id="diagRecoveryError">Нет</div></div>
    <div class="diag-item"><div class="diag-label">Последний CAN-кадр</div><div class="diag-value" id="diagCanAge">Никогда</div></div>
    <div class="diag-item"><div class="diag-label">Последний ответ ECM</div><div class="diag-value" id="diagEcmAge">Никогда</div></div>
    <div class="diag-item"><div class="diag-label">Последний ответ TCM</div><div class="diag-value" id="diagTcmAge">Никогда</div></div>
    <div class="diag-item"><div class="diag-label">Внешний свет</div><div class="diag-value" id="diagLight">Нет данных</div></div>
    <div class="diag-item"><div class="diag-label">Яркость подсветки</div><div class="diag-value" id="diagBrightness">Загрузка...</div><div class="diag-help">Регулировка работает не на всех дисплеях: требуется вход BLK</div></div>
  </div>
  <div class="reset-history"><div class="reset-history-title">Последние загрузки, новые слева</div><div class="reset-history-list" id="resetHistory"><span class="reset-history-item">Загрузка...</span></div></div>
</div>
<footer>Infiniti QX50 J55 Monitoring &mdash; ESP32</footer>
<script>
const POLL_MS=1000;
const LIGHT_STALE_MS=15000;
const statusEl=document.getElementById('status');
const statusText=document.getElementById('statusText');
let lastDeviceUptime=null;
const RESET_REASON_NAMES={power_on:'Включение питания',external:'Внешний сброс',software:'Программная перезагрузка',panic:'Сбой прошивки (panic)',interrupt_watchdog:'Watchdog прерываний',task_watchdog:'Watchdog задачи',watchdog:'Watchdog',deep_sleep:'Выход из сна',brownout:'Просадка питания',sdio:'Сброс SDIO',unknown:'Причина неизвестна'};
const TWAI_STATE_NAMES={not_installed:'Не инициализирован',stopped:'Остановлен',running:'Работает',bus_off:'Bus-Off',recovering:'Восстановление',unknown:'Состояние неизвестно'};
function fmtUptime(ms){let s=Math.floor(ms/1000);const h=Math.floor(s/3600);s-=h*3600;const m=Math.floor(s/60);s-=m*60;const pad=n=>String(n).padStart(2,'0');return pad(h)+':'+pad(m)+':'+pad(s);}
function fmtAge(ms){if(ms===null||ms===undefined)return 'Никогда';if(ms<1000)return Math.round(ms)+' мс назад';if(ms<60000)return(ms/1000).toFixed(1)+' с назад';return Math.floor(ms/60000)+' мин назад';}
function setAge(id,ageMs,staleMs){const el=document.getElementById(id);el.textContent=fmtAge(ageMs);el.className='diag-value '+(ageMs!==null&&ageMs<=staleMs?'ok':'danger');}
function isBadReset(reason){return reason==='brownout'||reason==='panic'||reason.includes('watchdog');}
function render(data){
  const reason=data.reset_reason||'unknown';const state=data.twai_state||'unknown';
  const busOffCount=Number(data.can_bus_off_count||0);const recoveryCount=Number(data.can_recovery_count||0);
  const recoveryAttempts=Number(data.can_recovery_attempt_count||0);const recoveryFailures=Number(data.can_recovery_failure_count||0);
  const restartAttempts=Number(data.can_restart_attempt_count||0);const restartFailures=Number(data.can_restart_failure_count||0);
  const reset=document.getElementById('diagReset');const twai=document.getElementById('diagTwai');
  const canCount=document.getElementById('diagCanCount');
  const busOff=document.getElementById('diagBusOff');const recovery=document.getElementById('diagRecovery');
  reset.textContent=RESET_REASON_NAMES[reason]||reason;reset.className='diag-value '+(isBadReset(reason)?'danger':'ok');
  twai.textContent=TWAI_STATE_NAMES[state]||state;twai.className='diag-value '+(state==='running'?'ok':(state==='recovering'?'warn':'danger'));
  canCount.textContent=String(Number(data.can_received_count||0));canCount.className='diag-value '+(Number(data.can_received_count||0)>0?'ok':'danger');
  busOff.textContent=String(busOffCount);busOff.className='diag-value '+(busOffCount>0?'danger':'ok');
  recovery.textContent=String(recoveryCount);recovery.className='diag-value '+(recoveryCount<busOffCount?'danger':(recoveryCount>0?'warn':'ok'));
  const recoveryAttemptsEl=document.getElementById('diagRecoveryAttempts');recoveryAttemptsEl.textContent=recoveryAttempts+' / '+recoveryFailures;recoveryAttemptsEl.className='diag-value '+(recoveryFailures>0?'danger':'ok');
  const restartAttemptsEl=document.getElementById('diagRestartAttempts');restartAttemptsEl.textContent=restartAttempts+' / '+restartFailures;restartAttemptsEl.className='diag-value '+(restartFailures>0?'danger':'ok');
  const recoveryError=document.getElementById('diagRecoveryError');const recoveryErrorCode=Number(data.can_last_recovery_error||0);recoveryError.textContent=recoveryErrorCode===0?'Нет':String(recoveryErrorCode);recoveryError.className='diag-value '+(recoveryErrorCode===0?'ok':'danger');
  const light=document.getElementById('diagLight');const lightAge=data.exterior_light_age_ms;
  if(lightAge===null||lightAge===undefined){light.textContent='Нет данных';light.className='diag-value danger';}
  else{light.textContent=(data.exterior_light_on?'Включен':'Выключен')+' · '+fmtAge(lightAge);light.className='diag-value '+(lightAge<=LIGHT_STALE_MS?'ok':'danger');}
  document.getElementById('diagBrightness').textContent=data.brightness_percent+'%';
  setAge('diagCanAge',data.can_last_rx_age_ms,data.stale_ms);setAge('diagEcmAge',data.ecm_last_response_age_ms,data.stale_ms);setAge('diagTcmAge',data.tcm_last_response_age_ms,data.stale_ms);
}
async function loadResetHistory(){
  const list=document.getElementById('resetHistory');
  try{const r=await fetch('/reset-history',{cache:'no-store'});if(!r.ok)throw new Error('HTTP '+r.status);const history=await r.json();
    if(!history.length){list.innerHTML='<span class="reset-history-item">История пуста</span>';return;}
    list.innerHTML=history.map((entry,index)=>{const reason=entry.reason||'unknown';const name=RESET_REASON_NAMES[reason]||reason;const prefix=index===0?'Сейчас':('−'+index);const cls=isBadReset(reason)?' danger':'';return '<span class="reset-history-item'+cls+'">'+prefix+': '+name+'</span>';}).join('');
  }catch(e){list.innerHTML='<span class="reset-history-item danger">История недоступна</span>';}
}
async function tick(){
  try{const r=await fetch('/metrics',{cache:'no-store'});if(!r.ok)throw new Error('HTTP '+r.status);const data=await r.json();render(data);statusEl.className='status online';statusText.textContent='В сети · v'+(data.version||'?')+' · аптайм '+fmtUptime(data.uptime_ms);const restarted=lastDeviceUptime!==null&&data.uptime_ms<lastDeviceUptime;lastDeviceUptime=data.uptime_ms;if(restarted)await loadResetHistory();}
  catch(e){statusEl.className='status offline';statusText.textContent='Нет связи с устройством';}
  finally{setTimeout(tick,POLL_MS);}
}
(async()=>{await tick();await loadResetHistory();})();
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
  <a class="nav-link" href="/health">&#128295; Работоспособность устройства</a>
  <a class="nav-link" href="/obd">OBD-II PID</a>
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

<footer><a href="https://github.com/oSkrobuk/infiniti-qx50-j55-monitoring" target="_blank" rel="noopener" style="color:var(--accent);text-decoration:none">Infiniti QX50 J55 Monitoring</a> &mdash; ESP32</footer>

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

// Обновить индикатор связи (версия прошивки приходит в том же ответе /metrics)
function setOnline(on, uptimeMs, version) {
  statusEl.className = 'status ' + (on ? 'online' : 'offline');
  if (on) {
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    const t = pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' + pad(now.getSeconds());
    statusText.textContent = 'В сети · v' + (version || '?') +
      ' · обновлено ' + t + ' · аптайм ' + fmtUptime(uptimeMs);
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
    setOnline(true, data.uptime_ms, data.version);
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

// Стартовые запросы идут последовательно, а не параллельно: HTTP-сервер
// обслуживает одно соединение за раз, и второй запрос на загрузке страницы
// просто ждал бы своей очереди, задерживая появление данных в карточках
(async () => {
  await tick();
  alertsTick();
})();
</script>
</body>
</html>
)rawhtml";

static const char OBD_HTML[] PROGMEM = R"rawhtml(
<!doctype html><html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kanvex — диагностика CAN</title><style>
:root{color-scheme:dark;--bg:#090d12;--card:#111821;--line:#293543;--gold:#d7aa55;--muted:#91a0af}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:#edf2f7;font:15px system-ui,sans-serif}
main{max-width:1000px;margin:auto;padding:24px}h1{font-size:24px;letter-spacing:.08em}h2{margin-top:28px}a{color:var(--gold)}
#status{color:var(--muted);margin:8px 0 20px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:10px}
.card{border:1px solid var(--line);border-radius:10px;background:var(--card);padding:14px}.pid{color:var(--gold);font:700 12px monospace}
.name{min-height:38px;margin:6px 0;color:var(--muted)}.value{font:600 25px ui-monospace,monospace}.stale{opacity:.55}
label{display:flex;gap:8px;align-items:center}.note{color:var(--muted);line-height:1.45}
</style></head><body><main><a href="/">&larr; К настройкам</a><h1>Диагностика CAN</h1>
<div id="status">Ожидание данных…</div><h2>Infiniti UDS DID</h2>
<p class="note">Основные DID уже читает монитор: страница не создает для них дополнительных запросов.</p>
<div id="didGrid" class="grid"></div><h2>Стандартные OBD-II PID</h2>
<p class="note">Отметьте нужные PID. По умолчанию все выключены, отправляется не более одного запроса в секунду.</p>
<div id="pidGrid" class="grid"></div></main><script>
const statusEl=document.getElementById('status'),didGrid=document.getElementById('didGrid'),pidGrid=document.getElementById('pidGrid');
const value=m=>(m.value===null?'—':m.value.toFixed(m.precision))+' <small>'+m.unit+'</small>';
function card(m,check){return '<div class="card '+(m.fresh?'':'stale')+'"><div class="pid">'+(check?'<label><input type="checkbox" data-pid="'+m.id+'" '+(m.enabled?'checked':'')+'>':'')+m.kind+' '+m.id+(check?'</label>':'')+'</div><div class="name">'+m.name+'</div><div class="value">'+value(m)+'</div></div>'}
function render(data){statusEl.textContent='Диагностический режим активен · один PID-запрос в секунду';
didGrid.innerHTML=data.dids.map(m=>card(m,false)).join('');pidGrid.innerHTML=data.pids.map(m=>card(m,true)).join('')}
async function update(){try{const r=await fetch('/obd-metrics',{cache:'no-store'});if(!r.ok)throw Error(r.status);render(await r.json())}catch(e){statusEl.textContent='Нет связи: '+e}}
pidGrid.addEventListener('change',async e=>{if(!e.target.matches('[data-pid]'))return;const body=new URLSearchParams({pid:e.target.dataset.pid,enabled:e.target.checked?'1':'0'});try{const r=await fetch('/obd-selection',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});if(!r.ok)throw Error(r.status)}catch(err){e.target.checked=!e.target.checked;statusEl.textContent='Не удалось изменить опрос: '+err}});
update();setInterval(update,1000);
</script></body></html>
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

// Состояние датчика давления масла — минимум зависит от текущих оборотов
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

// Состояние периода обновления RPM
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

struct ObdMetricDescriptor {
    uint8_t pid;
    const char *name;
    const char *unit;
    uint8_t precision;
};

static constexpr ObdMetricDescriptor OBD_DESCRIPTORS[] = {
    {0x04, "Расчетная нагрузка", "%", 1}, {0x05, "Температура ОЖ", "°C", 0},
    {0x06, "Краткая коррекция Bank 1", "%", 1}, {0x07, "Долгая коррекция Bank 1", "%", 1},
    {0x0A, "Давление топлива", "кПа", 0}, {0x0B, "Давление во впуске", "кПа", 0},
    {0x0C, "Обороты двигателя", "об/мин", 0}, {0x0D, "Скорость", "км/ч", 0},
    {0x0E, "Опережение зажигания", "°", 1}, {0x0F, "Температура воздуха", "°C", 0},
    {0x10, "Массовый расход воздуха", "г/с", 2}, {0x11, "Положение дросселя", "%", 1},
    {0x1F, "Время работы двигателя", "с", 0}, {0x23, "Давление в рампе", "кПа", 0},
    {0x24, "A/F Bank 1 Sensor 1", "λ", 3}, {0x25, "A/F Bank 1 Sensor 2", "λ", 3},
    {0x26, "A/F Bank 1 Sensor 3", "λ", 3}, {0x27, "A/F Bank 1 Sensor 4", "λ", 3},
    {0x2F, "Уровень топлива", "%", 1}, {0x33, "Атмосферное давление", "кПа", 0},
    {0x3C, "Катализатор B1S1", "°C", 1}, {0x3D, "Катализатор B1S2", "°C", 1},
    {0x42, "Напряжение блока", "В", 3}, {0x43, "Абсолютная нагрузка", "%", 1},
    {0x44, "Заданное соотношение смеси", "λ", 3}, {0x46, "Наружная температура", "°C", 0},
    {0x49, "Педаль газа D", "%", 1}, {0x4A, "Педаль газа E", "%", 1},
    {0x4B, "Педаль газа F", "%", 1}, {0x4C, "Заданный дроссель", "%", 1},
    {0x5C, "Температура масла ДВС", "°C", 0}, {0x5E, "Расход топлива", "л/ч", 2},
    {0x61, "Запрошенный момент", "%", 0}, {0x62, "Фактический момент", "%", 0},
    {0x63, "Опорный момент", "Н·м", 0}, {0x64, "Момент, точка 1", "%", 0},
};

struct DidMetricDescriptor {
    uint16_t did;
    const char *ecu;
    const char *name;
    const char *unit;
    uint8_t precision;
};

static constexpr DidMetricDescriptor DID_DESCRIPTORS[] = {
    {0x1201, "ECM", "Обороты двигателя", "об/мин", 0},
    {0x110E, "ECM", "Напряжение датчика наддува", "В", 2},
    {0x1278, "ECM", "Напряжение датчика давления масла", "В", 2},
    {0x1103, "ECM", "Напряжение бортовой сети", "В", 2},
    {0x1101, "ECM", "Температура ОЖ двигателя", "°C", 0},
    {0x111F, "ECM", "Температура масла двигателя", "°C", 0},
    {0x116B, "ECM", "Температура ОЖ радиатора", "°C", 0},
    {0x110C, "TCM", "Температура масла вариатора", "°C", 0},
    {0x0E07, "BCM", "Внешний свет (0 — выключен, 1 — включен)", "", 0},
};

static const ObdMetricDescriptor *find_obd_descriptor(uint8_t pid)
{
    for (const ObdMetricDescriptor &descriptor : OBD_DESCRIPTORS) {
        if (descriptor.pid == pid) return &descriptor;
    }
    return nullptr;
}

static ObdMetricValue obd_metric_value(uint8_t pid)
{
    switch (pid) {
        case 0x05: return {can_metrics.engine_coolant, can_metrics.engine_coolant_ts};
        case 0x0C: return {can_metrics.engine_rpm, can_metrics.engine_rpm_ts};
        case 0x42: return {can_metrics.battery_voltage, can_metrics.battery_voltage_ts};
        case 0x5C: return {can_metrics.engine_oil, can_metrics.engine_oil_ts};
        default: return can_metrics.obd[pid];
    }
}

static ObdMetricValue did_metric_value(uint16_t did)
{
    switch (did) {
        case 0x1201:
            return can_metrics.engine_rpm_source == MetricSource::INFINITI_UDS
                ? ObdMetricValue{can_metrics.engine_rpm, can_metrics.engine_rpm_ts} : ObdMetricValue{};
        case 0x110E: return {can_metrics.turbo_boost_volt, can_metrics.turbo_boost_volt_ts};
        case 0x1278: return {can_metrics.oil_pressure_volt, can_metrics.oil_pressure_volt_ts};
        case 0x1103:
            return can_metrics.battery_voltage_source == MetricSource::INFINITI_UDS
                ? ObdMetricValue{can_metrics.battery_voltage, can_metrics.battery_voltage_ts} : ObdMetricValue{};
        case 0x1101:
            return can_metrics.engine_coolant_source == MetricSource::INFINITI_UDS
                ? ObdMetricValue{can_metrics.engine_coolant, can_metrics.engine_coolant_ts} : ObdMetricValue{};
        case 0x111F:
            return can_metrics.engine_oil_source == MetricSource::INFINITI_UDS
                ? ObdMetricValue{can_metrics.engine_oil, can_metrics.engine_oil_ts} : ObdMetricValue{};
        case 0x116B: return {can_metrics.radiator_coolant, can_metrics.radiator_coolant_ts};
        case 0x110C: return {can_metrics.cvt_temp, can_metrics.cvt_temp_ts};
        case 0x0E07:
            return {can_metrics.exterior_light_on ? 1.0f : 0.0f, can_metrics.exterior_light_ts};
        default: return {};
    }
}

// ─────────────────────────────────────────────

// Не отдавать клиенту шлюз по DHCP — иначе телефон теряет мобильный интернет.
//
// По умолчанию dhcp-сервер softAP отдает опцию 3 (router) со своим адресом
// 192.168.4.1. Телефон ставит через него маршрут по умолчанию, весь трафик
// уходит в устройство, у которого интернета нет, и мобильные данные перестают
// работать до тех пор, пока система сама не решит, что сеть невалидна.
//
// Без опции 3 маршрута по умолчанию в нашей сети нет вообще: телефон оставляет
// основной сетью мобильную, а 192.168.4.1 остается доступен по on-link маршруту
// своей подсети. Именно это позволяет веб-интерфейсу спрашивать GitHub о новой
// версии прошивки, не отключаясь от устройства.
//
// Опцию 6 (DNS) dhcpserver из IDF 4.4 не умеет не отдавать: сброшенный флаг
// OFFER_DNS означает «отдать адрес самой точки доступа». Это не мешает — DNS
// сети без маршрута по умолчанию система для своих запросов не использует
static void ap_disable_dhcp_router_option()
{
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap == nullptr) {
        Serial.println("[WiFi] Интерфейс AP не найден — опции DHCP оставлены по умолчанию");
        return;
    }

    // Менять опции разрешено только на остановленном dhcp-сервере
    esp_netif_dhcps_stop(ap);

    uint8_t offer = 0; // dhcps_offer_t без флага OFFER_ROUTER
    esp_err_t err = esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
                                           ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
                                           &offer, sizeof(offer));
    if (err != ESP_OK) {
        Serial.printf("[WiFi] Опцию DHCP router отключить не удалось: %s\r\n", esp_err_to_name(err));
    }

    err = esp_netif_dhcps_start(ap);
    if (err != ESP_OK) {
        Serial.printf("[WiFi] DHCP-сервер не запустился: %s\r\n", esp_err_to_name(err));
        return;
    }

    Serial.println("[WiFi] Шлюз по DHCP не отдается — мобильный интернет у клиента сохраняется");
}

WebManager::WebManager()
    : server_(80), ota_upload_tag_(), ota_upload_error_(), ota_upload_ok_(false)
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

    ap_disable_dhcp_router_option();

    server_.on("/",            HTTP_GET,  [this]() { handle_root(); });
    server_.on("/live",        HTTP_GET,  [this]() { handle_live_page(); });
    server_.on("/obd",         HTTP_GET,  [this]() { handle_obd_page(); });
    server_.on("/health",      HTTP_GET,  [this]() { handle_health_page(); });
    server_.on("/metrics",     HTTP_GET,  [this]() { handle_get_metrics(); });
    server_.on("/obd-metrics", HTTP_GET,  [this]() { handle_get_obd_metrics(); });
    server_.on("/obd-selection", HTTP_POST, [this]() { handle_post_obd_selection(); });
    server_.on("/reset-history", HTTP_GET, [this]() { handle_get_reset_history(); });
    server_.on("/version",     HTTP_GET,  [this]() { handle_get_version(); });
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

    // Слоты прошивки
    server_.on("/slots",     HTTP_GET,  [this]() { handle_get_slots(); });
    server_.on("/boot-slot", HTTP_POST, [this]() { handle_post_boot_slot(); });

    // GET /update — та же страница, что и корень
    server_.on("/update", HTTP_GET, [this]() { handle_update_page(); });

    // POST /update — загрузка .bin (два обработчика: завершение + чанки данных)
    server_.on("/update", HTTP_POST,
        [this]() { // вызывается ПОСЛЕ завершения загрузки файла
            server_.sendHeader("Connection", "close");
            if (ota_upload_ok_) {
                server_.send(200, "application/json", "{\"ok\":true}");
                Serial.println("[OTA] Успех — перезагрузка...");
                delay(300);
                ESP.restart();
            } else {
                const bool validation_error = !ota_upload_error_.isEmpty();
                String err = validation_error ? ota_upload_error_ : Update.errorString();
                server_.send(validation_error ? 400 : 500, "application/json",
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
    server_.handle_client();
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

void WebManager::handle_obd_page()
{
    server_.send_P(200, "text/html; charset=utf-8", OBD_HTML);
}

void WebManager::handle_health_page()
{
    server_.send_P(200, "text/html; charset=utf-8", HEALTH_HTML);
}

void WebManager::handle_get_version()
{
    // Слот, из которого загрузилась работающая прошивка: ota_0 или ota_1
    const esp_partition_t *running = esp_ota_get_running_partition();
    const char *slot = (running != nullptr && running->label[0] != '\0') ? running->label : "?";

    String json;
    json.reserve(192);
    json += "{\"version\":\"";
    json += FW_VERSION;
    json += "\",\"build\":\"";
    json += __DATE__ " " __TIME__;
    json += "\",\"slot\":\"";
    json += slot;
    json += "\",\"env\":\"";
    json += BUILD_ENV;
    json += "\",\"uptime_s\":";
    json += String(millis() / 1000);
    json += "}";

    server_.send(200, "application/json", json);
}

void WebManager::handle_get_metrics()
{
    const uint32_t stale_ms = static_cast<uint32_t>(config.get("system", "stale_ms"));

    // Обороты нужны для расчёта состояния давления масла и периода опроса
    const bool  rpm_fresh = metric_fresh(can_metrics.engine_rpm_ts, stale_ms);
    const float rpm       = rpm_fresh ? can_metrics.engine_rpm : 0.0f;

    String json;
    json.reserve(1850);
    json += "{\"version\":\"";
    json += FW_VERSION;
    json += "\",\"uptime_ms\":";
    json += String(millis());
    json += ",\"stale_ms\":";
    json += String(stale_ms);
    json += ",\"brightness_percent\":";
    json += String(config.get("system", "brightness_percent"), 0);
    json += ",\"reset_reason\":\"";
    json += reset_history.current_reason_name();
    json += "\",\"reset_reason_code\":";
    json += String(static_cast<int>(reset_history.current_reason()));
    json += ",\"twai_state\":\"";
    json += can_bus.state_name();
    json += "\",\"can_bus_off_count\":";
    json += String(can_bus.bus_off_count());
    json += ",\"can_recovery_count\":";
    json += String(can_bus.recovery_count());
    json += ",\"can_recovery_attempt_count\":";
    json += String(can_bus.recovery_attempt_count());
    json += ",\"can_recovery_failure_count\":";
    json += String(can_bus.recovery_failure_count());
    json += ",\"can_restart_attempt_count\":";
    json += String(can_bus.restart_attempt_count());
    json += ",\"can_restart_failure_count\":";
    json += String(can_bus.restart_failure_count());
    json += ",\"can_last_recovery_error\":";
    json += String(can_bus.last_recovery_error());
    json += ",\"can_received_count\":";
    json += String(can_bus.received_count());
    const uint32_t now = millis();
    const uint32_t last_rx_ts = can_bus.last_rx_ts();
    const uint32_t last_ecm_response_ts = can_bus.last_ecm_response_ts();
    const uint32_t last_tcm_response_ts = can_bus.last_tcm_response_ts();
    const uint32_t exterior_light_ts = can_metrics.exterior_light_ts;
    json += ",\"can_last_rx_age_ms\":";
    json += last_rx_ts == 0 ? "null" : String(now - last_rx_ts);
    json += ",\"ecm_last_response_age_ms\":";
    json += last_ecm_response_ts == 0 ? "null" : String(now - last_ecm_response_ts);
    json += ",\"tcm_last_response_age_ms\":";
    json += last_tcm_response_ts == 0 ? "null" : String(now - last_tcm_response_ts);
    json += ",\"exterior_light_on\":";
    json += exterior_light_ts == 0 ? "null" : (can_metrics.exterior_light_on ? "true" : "false");
    json += ",\"exterior_light_age_ms\":";
    json += exterior_light_ts == 0 ? "null" : String(now - exterior_light_ts);
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
        append_metric(json, "oil_pressure", "OIL-PR-V", "Датчик давления масла", "В",
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
        // Период обновления RPM не ограничиваем stale_ms — это результат последнего замера,
        // а не живой поток (см. main.cpp)
        const bool f = (can_metrics.rpm_poll_time_ts != 0);
        append_metric(json, "poll_time", "RPM-POLL", "Период обновления RPM", "с",
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

void WebManager::handle_get_obd_metrics()
{
    diagnostic_mode_touch(millis());
    const uint32_t stale_ms = static_cast<uint32_t>(config.get("system", "stale_ms"));
    String json;
    json.reserve(12000);
    json += "{\"stale_ms\":";
    json += String(stale_ms);
    json += ",\"dids\":[";

    bool first = true;
    for (const DidMetricDescriptor &descriptor : DID_DESCRIPTORS) {
        const ObdMetricValue metric = did_metric_value(descriptor.did);
        if (!first) json += ',';
        first = false;
        char did[5];
        snprintf(did, sizeof(did), "%04X", descriptor.did);
        json += "{\"kind\":\"";
        json += descriptor.ecu;
        json += " DID\",\"id\":\"";
        json += did;
        json += "\",\"name\":\"";
        json += descriptor.name;
        json += "\",\"unit\":\"";
        json += descriptor.unit;
        json += "\",\"precision\":";
        json += String(descriptor.precision);
        json += ",\"value\":";
        json += metric.ts == 0
            ? "null"
            : String(metric.value, static_cast<unsigned int>(descriptor.precision));
        json += ",\"fresh\":";
        json += metric_fresh(metric.ts, stale_ms) ? "true" : "false";
        json += '}';
    }

    json += "],\"pids\":[";
    first = true;
    for (uint16_t raw_pid = 1; raw_pid <= DIAGNOSTIC_MAX_PID; ++raw_pid) {
        const uint8_t pid = static_cast<uint8_t>(raw_pid);
        if ((pid & 0x1F) == 0 || !obd_pid_catalog.supports(pid)) continue;
        const ObdMetricDescriptor *descriptor = find_obd_descriptor(pid);
        const ObdMetricValue metric = obd_metric_value(pid);
        if (!first) json += ',';
        first = false;
        char id[3];
        snprintf(id, sizeof(id), "%02X", pid);
        json += "{\"kind\":\"PID\",\"id\":\"";
        json += id;
        json += "\",\"name\":\"";
        json += descriptor == nullptr ? "Стандартный параметр (raw)" : descriptor->name;
        json += "\",\"unit\":\"";
        json += descriptor == nullptr ? "raw" : descriptor->unit;
        json += "\",\"precision\":";
        json += String(descriptor == nullptr ? 0 : descriptor->precision);
        json += ",\"enabled\":";
        json += diagnostic_selection.pid_enabled(pid) ? "true" : "false";
        json += ",\"value\":";
        json += metric.ts == 0
            ? "null"
            : String(metric.value, static_cast<unsigned int>(descriptor == nullptr ? 0 : descriptor->precision));
        json += ",\"fresh\":";
        json += metric_fresh(metric.ts, stale_ms) ? "true" : "false";
        json += '}';
    }
    json += "]}";
    server_.send(200, "application/json", json);
}

void WebManager::handle_post_obd_selection()
{
    if (!server_.hasArg("pid") || !server_.hasArg("enabled")) {
        server_.send(400, "application/json", "{\"error\":\"pid and enabled are required\"}");
        return;
    }

    unsigned int pid = 0;
    if (sscanf(server_.arg("pid").c_str(), "%x", &pid) != 1 || pid == 0 || pid > DIAGNOSTIC_MAX_PID ||
        (pid & 0x1F) == 0) {
        server_.send(400, "application/json", "{\"error\":\"invalid pid\"}");
        return;
    }

    const String enabled = server_.arg("enabled");
    if (enabled != "0" && enabled != "1") {
        server_.send(400, "application/json", "{\"error\":\"invalid enabled value\"}");
        return;
    }

    diagnostic_selection.set_pid(static_cast<uint8_t>(pid), enabled == "1");
    diagnostic_mode_touch(millis());
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WebManager::handle_get_reset_history()
{
    server_.send(200, "application/json", reset_history.to_json());
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
        ota_upload_tag_.reset();
        ota_upload_error_ = "";
        ota_upload_ok_    = false;
        Serial.printf("[OTA] Начало: %s\r\n", upload.filename.c_str());
        // UPDATE_SIZE_UNKNOWN — Update сам определит конец по закрытию соединения
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.printf("[OTA] begin() ошибка: %s\r\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_WRITE:
        ota_upload_tag_.feed(upload.buf, upload.currentSize);
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Serial.printf("[OTA] write() ошибка: %s\r\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_END:
        if (!Update.isRunning()) break;

        if (!ota_upload_tag_.found()) {
            ota_upload_error_ = "В образе нет маркера QX50-FW-TAG";
            Update.abort();
            Serial.println("[OTA] Образ отклонен: маркер QX50-FW-TAG не найден");
            break;
        }

        if (!ota_envs_compatible(BUILD_ENV, ota_upload_tag_.tag().env.c_str())) {
            ota_upload_error_ = "Прошивка предназначена для другой платы";
            Serial.printf("[OTA] Образ отклонен: устройство %s, образ %s\r\n",
                          BUILD_ENV, ota_upload_tag_.tag().env.c_str());
            Update.abort();
            break;
        }

        if (Update.end(true)) { // true = финализировать (проверить MD5/размер)
            ota_upload_ok_ = true;
            Serial.printf("[OTA] Завершено: %u байт\r\n", upload.totalSize);
        } else {
            Serial.printf("[OTA] end() ошибка: %s\r\n", Update.errorString());
        }
        break;

    case UPLOAD_FILE_ABORTED:
        if (Update.isRunning()) Update.abort();
        ota_upload_error_ = "Загрузка прервана";
        Serial.println("[OTA] Загрузка прервана");
        break;

    default:
        break;
    }
}

void WebManager::handle_get_slots()
{
    OtaSlotInfo slots[2];
    const size_t count = ota_slots_collect(slots, 2);

    String json;
    json.reserve(512);
    json += "{\"slots\":[";

    for (size_t i = 0; i < count; i++) {
        const OtaSlotInfo &s = slots[i];

        if (i > 0) json += ',';

        json += "{\"label\":\"";
        json += s.label;
        json += "\",\"valid\":";
        json += s.valid ? "true" : "false";
        json += ",\"running\":";
        json += s.running ? "true" : "false";
        json += ",\"boot\":";
        json += s.boot ? "true" : "false";
        // Версия, окружение и дата сборки известны только по маркеру в образе:
        // прошивка старее этой функции маркера не содержит, и поля остаются пустыми
        json += ",\"version\":\"";
        if (s.known) json += s.version;
        json += "\",\"env\":\"";
        if (s.known) json += s.env;
        json += "\",\"build\":\"";
        if (s.known) json += s.build;
        json += "\",\"size\":";
        json += String(s.size);
        json += ",\"used\":";
        json += String(s.used);
        json += '}';
    }

    json += "]}";
    server_.send(200, "application/json", json);
}

void WebManager::handle_post_boot_slot()
{
    String err;

    if (!ota_slots_set_boot(server_.arg("slot").c_str(), err)) {
        server_.send(400, "application/json", "{\"error\":\"" + err + "\"}");
        return;
    }

    server_.send(200, "application/json", "{\"ok\":true}");
}
