#include "WebManager.h"
#include "ConfigManager.h"
#include <WiFi.h>

// HTML страница хранится во флеш-памяти (PROGMEM), не занимает RAM
// Минифицирована для экономии флеша ESP32
static const char INDEX_HTML[] PROGMEM =
"<!DOCTYPE html><html lang=ru><head><meta charset=UTF-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>QX50 Config</title><style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{background:#0d0d0d;color:#e0e0e0;font-family:Arial,sans-serif;padding:16px}"
"h1{text-align:center;color:#c9a84c;letter-spacing:3px;text-transform:uppercase;font-size:1.3rem;padding:12px 0 4px}"
"p.sub{text-align:center;color:#888;font-size:.8rem;margin-bottom:16px}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:14px;margin-bottom:16px}"
".card{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:8px;padding:14px}"
".card h2{color:#c9a84c;font-size:.8rem;letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}"
".row{margin-bottom:10px}"
".row label{display:block;font-size:.75rem;color:#888;margin-bottom:3px}"
".row input{width:100%;background:#111;border:1px solid #2a2a2a;border-radius:5px;color:#e0e0e0;font-size:.95rem;padding:6px 10px;outline:none}"
".row input:focus{border-color:#c9a84c}"
".leg{display:flex;gap:12px;font-size:.75rem;color:#888;margin-bottom:14px;flex-wrap:wrap}"
".leg span{display:flex;align-items:center;gap:4px}"
".d{width:8px;height:8px;border-radius:50%;display:inline-block}"
".acts{display:flex;gap:10px}"
"button{flex:1;padding:11px;border:none;border-radius:7px;font-size:.9rem;font-weight:700;cursor:pointer}"
"button:disabled{opacity:.4}"
".bs{background:#c9a84c;color:#000}"
".br{background:#2a2a2a;color:#e0e0e0}"
".toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%) translateY(80px);background:#1a1a1a;border:1px solid #2a2a2a;border-radius:7px;padding:10px 20px;font-size:.85rem;transition:transform .3s;z-index:9}"
".toast.show{transform:translateX(-50%) translateY(0)}"
".ok{border-color:#4caf50;color:#4caf50}.err{border-color:#f44336;color:#f44336}"
"</style></head><body>"
"<h1>&#9670; Infiniti QX50 J55</h1>"
"<p class=sub>Temperature Config Editor</p>"
"<div class=leg>"
"<span><span class=d style=background:#2196f3></span>Min</span>"
"<span><span class=d style=background:#4caf50></span>Target</span>"
"<span><span class=d style=background:#f44336></span>Max</span>"
"</div>"
"<form id=F><div class=grid>"
"<div class=card><h2>&#128167; E-OIL &mdash; Engine Oil</h2>"
"<div class=row><label>Min &deg;C</label><input type=number step=0.5 name=oil_min required></div>"
"<div class=row><label>Target &deg;C</label><input type=number step=0.5 name=oil_target required></div>"
"<div class=row><label>Max &deg;C</label><input type=number step=0.5 name=oil_max required></div></div>"
"<div class=card><h2>&#10052; E-COOL &mdash; Engine Coolant</h2>"
"<div class=row><label>Min &deg;C</label><input type=number step=0.5 name=coolant_min required></div>"
"<div class=row><label>Target &deg;C</label><input type=number step=0.5 name=coolant_target required></div>"
"<div class=row><label>Max &deg;C</label><input type=number step=0.5 name=coolant_max required></div></div>"
"<div class=card><h2>&#127777; R-COOL &mdash; Radiator Coolant</h2>"
"<div class=row><label>Min &deg;C</label><input type=number step=0.5 name=radiator_min required></div>"
"<div class=row><label>Target &deg;C</label><input type=number step=0.5 name=radiator_target required></div>"
"<div class=row><label>Max &deg;C</label><input type=number step=0.5 name=radiator_max required></div></div>"
"<div class=card><h2>&#9881; T-OIL &mdash; Transmission Oil</h2>"
"<div class=row><label>Min &deg;C</label><input type=number step=0.5 name=transmission_min required></div>"
"<div class=row><label>Target &deg;C</label><input type=number step=0.5 name=transmission_target required></div>"
"<div class=row><label>Max &deg;C</label><input type=number step=0.5 name=transmission_max required></div></div>"
"</div><div class=acts>"
"<button type=button class=br id=bR>&#8635; Reset</button>"
"<button type=submit class=bs id=bS>&#10003; Save</button>"
"</div></form>"
"<div class=toast id=T></div>"
"<script>"
"var orig={};"
"function toast(m,t){var e=document.getElementById('T');e.textContent=m;e.className='toast '+t+' show';setTimeout(()=>e.className='toast',3000);}"
"function fill(c){['oil','coolant','radiator','transmission'].forEach(function(f){"
"['min','target','max'].forEach(function(s){"
"var el=document.querySelector('[name='+f+'_'+s+']');if(el)el.value=c[f][s];});});}"
"function read(){var d={};['oil','coolant','radiator','transmission'].forEach(function(s){"
"d[s]={min:+document.querySelector('[name='+s+'_min]').value,"
"target:+document.querySelector('[name='+s+'_target]').value,"
"max:+document.querySelector('[name='+s+'_max]').value};});return d;}"
"function validate(d){var ss=['oil','coolant','radiator','transmission'];"
"for(var i=0;i<ss.length;i++){var s=ss[i];"
"if(d[s].min>=d[s].target)return s+': min must be < target';"
"if(d[s].target>=d[s].max)return s+': target must be < max';}return null;}"
"fetch('/config').then(r=>r.json()).then(c=>{orig=JSON.parse(JSON.stringify(c));fill(c);})"
".catch(e=>toast('Load error: '+e,'err'));"
"document.getElementById('F').addEventListener('submit',function(e){"
"e.preventDefault();var d=read(),err=validate(d);"
"if(err){toast(err,'err');return;}"
"var b=document.getElementById('bS');b.disabled=true;"
"fetch('/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})"
".then(r=>{if(!r.ok)throw r.status;orig=JSON.parse(JSON.stringify(d));toast('Saved!','ok');})"
".catch(e=>toast('Save error: '+e,'err')).finally(()=>b.disabled=false);});"
"document.getElementById('bR').onclick=function(){fill(orig);toast('Reset to saved','ok');};"
"</script></body></html>";

// ─────────────────────────────────────────────

WebManager::WebManager(const char *ssid, const char *password)
    : _ssid(ssid), _password(password), _server(80)
{
}

void WebManager::begin()
{
    // Поднимаем точку доступа
    WiFi.softAP(_ssid, _password);
    Serial.printf("WiFi AP запущен: SSID='%s'  IP=%s\n",
                  _ssid, WiFi.softAPIP().toString().c_str());

    // Регистрируем маршруты
    _server.on("/",       HTTP_GET,  [this]() { handleRoot(); });
    _server.on("/config", HTTP_GET,  [this]() { handleGetConfig(); });
    _server.on("/config", HTTP_POST, [this]() { handlePostConfig(); });
    _server.onNotFound(             [this]() { handleNotFound(); });

    _server.begin();
    Serial.println("HTTP сервер запущен на порту 80.");
}

void WebManager::handle()
{
    _server.handleClient();
}

String WebManager::getIP() const
{
    return WiFi.softAPIP().toString();
}

// ── Обработчики ──────────────────────────────

void WebManager::handleRoot()
{
    // Отдаём HTML из PROGMEM
    _server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebManager::handleGetConfig()
{
    // Отдаём текущий конфиг как JSON
    _server.send(200, "application/json", config.toJson());
}

void WebManager::handlePostConfig()
{
    if (!_server.hasArg("plain"))
    {
        _server.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    const String &body = _server.arg("plain");
    if (config.fromJson(body))
    {
        Serial.println("Конфиг обновлён через веб-интерфейс.");
        _server.send(200, "application/json", "{\"ok\":true}");
    }
    else
    {
        _server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    }
}

void WebManager::handleNotFound()
{
    _server.send(404, "text/plain", "Not found");
}
