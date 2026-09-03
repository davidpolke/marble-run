#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_APDS9960.h>
#include <math.h>
#include "config.h"

const char* WIFI_SSID = "Look Mum, No Wires!";
const char* WIFI_PASS = "Stocki?-8932Random";

Adafruit_APDS9960 apds;
WebServer server(80);

#define PROXIMITY_THRESHOLD 20

// ── Live sensor state ────────────────────────────────────────────────────────
struct ColorData { uint16_t r, g, b, c; uint8_t r8, g8, b8; };
ColorData live    = {};
uint8_t proximity = 0;
bool objectPresent = false;

// ── Saved color list ─────────────────────────────────────────────────────────
struct SavedColor { uint8_t r, g, b; uint32_t id; bool used; };
SavedColor saved[MAX_SAVED_COLORS] = {};
uint32_t nextId  = 1;
int32_t  matchId = -1;   // id of currently matching saved color, -1 = none

static uint8_t toU8(uint16_t v, uint16_t c) {
    if (!c) return 0;
    uint32_t s = (uint32_t)v * 255 / c;
    return s > 255 ? 255 : (uint8_t)s;
}

static float colorDist(uint8_t r1, uint8_t g1, uint8_t b1,
                        uint8_t r2, uint8_t g2, uint8_t b2) {
    float dr = (float)r1 - r2, dg = (float)g1 - g2, db = (float)b1 - b2;
    return sqrtf(dr*dr + dg*dg + db*db);
}

static int32_t findMatch(uint8_t r, uint8_t g, uint8_t b) {
    float best = MATCH_TOLERANCE;
    int32_t id = -1;
    for (int i = 0; i < MAX_SAVED_COLORS; i++) {
        if (!saved[i].used) continue;
        float d = colorDist(r, g, b, saved[i].r, saved[i].g, saved[i].b);
        if (d < best) { best = d; id = saved[i].id; }
    }
    return id;
}

// ── HTML ─────────────────────────────────────────────────────────────────────
static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Marble Run</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0c0c10;color:#e8e8f0;min-height:100vh}
header{padding:1.2rem 1.75rem;border-bottom:1px solid #1e1e2e;display:flex;align-items:center;gap:.7rem}
header h1{font-size:1.1rem;font-weight:600;letter-spacing:.04em}
.blink{width:8px;height:8px;border-radius:50%;background:#4ade80;animation:blink 2s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.layout{display:grid;grid-template-columns:340px 1fr;gap:0;min-height:calc(100vh - 53px)}
@media(max-width:700px){.layout{grid-template-columns:1fr}}
/* ── left panel ── */
.live{border-right:1px solid #1e1e2e;padding:1.5rem;display:flex;flex-direction:column;gap:1.25rem}
.section-title{font-size:.65rem;letter-spacing:.12em;text-transform:uppercase;color:#555;margin-bottom:.5rem}
.prox-row{display:flex;align-items:center;gap:.75rem}
.prox-track{flex:1;height:8px;background:#1a1a24;border-radius:4px;overflow:hidden}
.prox-fill{height:100%;border-radius:4px;background:linear-gradient(90deg,#818cf8,#a855f7);transition:width .12s}
.prox-num{font-size:.85rem;font-family:monospace;color:#a78bfa;width:2.5rem;text-align:right}
.color-card{border-radius:16px;overflow:hidden;border:1px solid #2a2a3e;transition:opacity .25s}
.color-card.idle{opacity:.3}
.swatch{height:140px;display:flex;align-items:center;justify-content:center;transition:background .15s}
.swatch-label{font-size:.75rem;letter-spacing:.1em;color:#ffffff44}
.card-info{padding:1.1rem 1.25rem;background:#16161f}
.hex-big{font-size:1.75rem;font-weight:700;font-family:monospace;margin-bottom:1rem}
.bars{display:flex;flex-direction:column;gap:.5rem}
.bar-row{display:flex;align-items:center;gap:.6rem}
.bar-lbl{font-size:.72rem;font-weight:800;width:.85rem}
.bar-bg{flex:1;height:6px;background:#222233;border-radius:3px;overflow:hidden}
.bar-fill{height:100%;border-radius:3px;transition:width .15s}
.bar-val{font-size:.75rem;font-family:monospace;color:#666;width:2.2rem;text-align:right}
.save-btn{width:100%;padding:.85rem;border:none;border-radius:12px;font-size:.9rem;font-weight:700;letter-spacing:.05em;cursor:pointer;transition:all .15s;background:#22c55e;color:#fff}
.save-btn:hover{background:#16a34a;transform:translateY(-1px)}
.save-btn:active{transform:translateY(0)}
.save-btn:disabled{background:#1a2a1e;color:#2d4a34;cursor:default;transform:none}
/* ── right panel ── */
.colors-panel{padding:1.5rem;overflow-y:auto}
.colors-panel .section-title{margin-bottom:1rem}
.empty-hint{color:#333;font-size:.85rem;margin-top:2rem;text-align:center}
.colors-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:.85rem}
.color-item{border-radius:14px;overflow:hidden;border:2px solid #2a2a3e;transition:border-color .2s,box-shadow .2s;position:relative}
.color-item.match{border-color:#4ade80;box-shadow:0 0 18px #4ade8044}
.item-swatch{height:80px;width:100%}
.item-body{padding:.65rem .75rem;background:#16161f;display:flex;align-items:center;gap:.4rem}
.item-hex{font-size:.75rem;font-family:monospace;color:#aaa;flex:1;overflow:hidden;text-overflow:ellipsis}
.del-btn{background:none;border:none;color:#444;font-size:1.1rem;cursor:pointer;line-height:1;padding:0 .1rem;transition:color .15s}
.del-btn:hover{color:#ef4444}
.match-badge{position:absolute;top:.45rem;right:.45rem;background:#4ade80;color:#052e16;font-size:.6rem;font-weight:800;letter-spacing:.06em;padding:.15rem .4rem;border-radius:999px}
</style>
</head>
<body>
<header><div class="blink"></div><h1>Marble Run</h1></header>
<div class="layout">
  <!-- live sensor -->
  <div class="live">
    <div>
      <div class="section-title">Proximity</div>
      <div class="prox-row">
        <div class="prox-track"><div class="prox-fill" id="prox-fill" style="width:0%"></div></div>
        <span class="prox-num" id="prox-num">0</span>
      </div>
    </div>
    <div class="color-card idle" id="color-card">
      <div class="swatch" id="swatch" style="background:#111118">
        <span class="swatch-label" id="swatch-label">WAITING FOR OBJECT</span>
      </div>
      <div class="card-info">
        <div class="hex-big" id="hex">#——————</div>
        <div class="bars">
          <div class="bar-row"><span class="bar-lbl" style="color:#f87171">R</span><div class="bar-bg"><div class="bar-fill" id="rb" style="background:#f87171;width:0%"></div></div><span class="bar-val" id="rv">—</span></div>
          <div class="bar-row"><span class="bar-lbl" style="color:#4ade80">G</span><div class="bar-bg"><div class="fill bar-fill" id="gb" style="background:#4ade80;width:0%"></div></div><span class="bar-val" id="gv">—</span></div>
          <div class="bar-row"><span class="bar-lbl" style="color:#60a5fa">B</span><div class="bar-bg"><div class="fill bar-fill" id="bb" style="background:#60a5fa;width:0%"></div></div><span class="bar-val" id="bv">—</span></div>
        </div>
      </div>
    </div>
    <button class="save-btn" id="save-btn" disabled onclick="saveColor()">Save Color</button>
  </div>
  <!-- saved list -->
  <div class="colors-panel">
    <div class="section-title">Saved Colors</div>
    <div class="empty-hint" id="empty-hint">No colors saved yet.<br>Hold an object in front of the sensor and press Save.</div>
    <div class="colors-grid" id="colors-grid"></div>
  </div>
</div>
<script>
const h2=n=>n.toString(16).padStart(2,'0');
let colors=[], currentPresent=false;

function renderColors(matchId){
  const grid=document.getElementById('colors-grid');
  const hint=document.getElementById('empty-hint');
  hint.style.display=colors.length?'none':'block';
  grid.innerHTML=colors.map(c=>{
    const hex='#'+h2(c.r8)+h2(c.g8)+h2(c.b8);
    const isMatch=c.id===matchId;
    return `<div class="color-item${isMatch?' match':''}" id="ci-${c.id}">
      ${isMatch?'<span class="match-badge">MATCH</span>':''}
      <div class="item-swatch" style="background:${hex}"></div>
      <div class="item-body">
        <span class="item-hex">${hex.toUpperCase()}</span>
        <button class="del-btn" onclick="deleteColor(${c.id})" title="Delete">×</button>
      </div>
    </div>`;
  }).join('');
}

async function loadColors(matchId){
  const res=await fetch('/colors');
  colors=await res.json();
  renderColors(matchId);
}

async function saveColor(){
  if(!currentPresent)return;
  await fetch('/save',{method:'POST'});
  await loadColors(-1);
}

async function deleteColor(id){
  await fetch('/delete?id='+id);
  await loadColors(-1);
}

async function poll(){
  try{
    const d=await(await fetch('/data')).json();
    currentPresent=d.present;
    document.getElementById('prox-num').textContent=d.proximity;
    document.getElementById('prox-fill').style.width=(d.proximity/255*100)+'%';
    const card=document.getElementById('color-card');
    const btn=document.getElementById('save-btn');
    const lbl=document.getElementById('swatch-label');
    if(d.present){
      card.classList.remove('idle');
      lbl.style.display='none';
      btn.disabled=false;
      const hex='#'+h2(d.r8)+h2(d.g8)+h2(d.b8);
      document.getElementById('swatch').style.background=hex;
      document.getElementById('hex').textContent=hex.toUpperCase();
      document.getElementById('rv').textContent=d.r8;
      document.getElementById('gv').textContent=d.g8;
      document.getElementById('bv').textContent=d.b8;
      document.getElementById('rb').style.width=(d.r8/255*100)+'%';
      document.getElementById('gb').style.width=(d.g8/255*100)+'%';
      document.getElementById('bb').style.width=(d.b8/255*100)+'%';
    } else {
      card.classList.add('idle');
      lbl.style.display='';
      btn.disabled=true;
      document.getElementById('swatch').style.background='#111118';
      document.getElementById('hex').textContent='#——————';
    }
    renderColors(d.matchId);
  }catch(_){}
  setTimeout(poll,300);
}

loadColors(-1);
poll();
</script>
</body>
</html>
)rawliteral";

// ── Endpoints ────────────────────────────────────────────────────────────────
void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

void handleData() {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"proximity\":%u,\"present\":%s,"
        "\"r8\":%u,\"g8\":%u,\"b8\":%u,"
        "\"r\":%u,\"g\":%u,\"b\":%u,\"c\":%u,"
        "\"matchId\":%d}",
        proximity, objectPresent ? "true" : "false",
        live.r8, live.g8, live.b8,
        live.r, live.g, live.b, live.c,
        (int)matchId);
    server.send(200, "application/json", buf);
}

void handleColors() {
    String json = "[";
    bool first = true;
    for (int i = 0; i < MAX_SAVED_COLORS; i++) {
        if (!saved[i].used) continue;
        if (!first) json += ",";
        char entry[64];
        snprintf(entry, sizeof(entry),
            "{\"id\":%u,\"r8\":%u,\"g8\":%u,\"b8\":%u}",
            saved[i].id, saved[i].r, saved[i].g, saved[i].b);
        json += entry;
        first = false;
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleSave() {
    if (!objectPresent) { server.send(400, "text/plain", "no object"); return; }
    for (int i = 0; i < MAX_SAVED_COLORS; i++) {
        if (saved[i].used) continue;
        saved[i] = { live.r8, live.g8, live.b8, nextId++, true };
        Serial.printf("[SAVE] #%02X%02X%02X id=%u\n",
            live.r8, live.g8, live.b8, saved[i].id);
        server.send(200, "text/plain", "ok");
        return;
    }
    server.send(507, "text/plain", "list full");
}

void handleDelete() {
    if (!server.hasArg("id")) { server.send(400, "text/plain", "missing id"); return; }
    uint32_t id = (uint32_t)server.arg("id").toInt();
    for (int i = 0; i < MAX_SAVED_COLORS; i++) {
        if (saved[i].used && saved[i].id == id) {
            saved[i].used = false;
            Serial.printf("[DELETE] id=%u\n", id);
            server.send(200, "text/plain", "ok");
            return;
        }
    }
    server.send(404, "text/plain", "not found");
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);

    if (!apds.begin(10, APDS9960_AGAIN_4X, APDS9960_ADDRESS, &Wire)) {
        Serial.println("[ERROR] APDS-9960 not found"); while (true) delay(100);
    }
    apds.enableProximity(true);
    apds.enableColor(true);
    Serial.println("[OK] APDS-9960");

    Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.printf("\nConnected!  →  http://%s\n", WiFi.localIP().toString().c_str());

    server.on("/",       handleRoot);
    server.on("/data",   handleData);
    server.on("/colors", handleColors);
    server.on("/save",   HTTP_POST, handleSave);
    server.on("/delete", handleDelete);
    server.begin();
    Serial.println("Web server running.");
}

// ── Loop ─────────────────────────────────────────────────────────────────────
unsigned long lastRead = 0;

void loop() {
    server.handleClient();

    if (millis() - lastRead < 100) return;
    lastRead = millis();

    proximity     = apds.readProximity();
    objectPresent = proximity > PROXIMITY_THRESHOLD;
    digitalWrite(LED_PIN, objectPresent ? HIGH : LOW);

    if (objectPresent && apds.colorDataReady()) {
        apds.getColorData(&live.r, &live.g, &live.b, &live.c);
        live.r8 = toU8(live.r, live.c);
        live.g8 = toU8(live.g, live.c);
        live.b8 = toU8(live.b, live.c);
        matchId = findMatch(live.r8, live.g8, live.b8);
    } else {
        matchId = -1;
    }
}
