/*
  Diorama Slave ESP32 — RGB Light Controller
  ─────────────────────────────────────────────────────────────────────────────
  Connects to the master ESP32 AP (Diorama-Park) as a WiFi station.
  Receives RGB commands via HTTP GET and drives 3 independent RGB LED groups.

  ── Inverted PWM (MOSFET drive) ──────────────────────────────────────────────
    PWM 0   = full brightness (LED ON)
    PWM 255 = fully off
  Master sends logical RGB (0-255) + brightness (0-100).
  Slave computes: pin_duty = 255 - (color * brightness / 100)

  ── Wiring ───────────────────────────────────────────────────────────────────
  Group 1  Left Fountain  : R=GPIO4,  G=GPIO5,  B=GPIO6
  Group 2  Right Fountain : R=GPIO7,  G=GPIO22, B=GPIO25
  Group 3  Inner Circle   : R=GPIO26, G=GPIO27, B=GPIO8

  ── API (master calls these) ─────────────────────────────────────────────────
  GET /rgb?group=1|2|3|all&r=0-255&g=0-255&b=0-255&brightness=0-100
  GET /off          → all LEDs off
  GET /status       → JSON state
*/

#include <WiFi.h>
#include <WebServer.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "Diorama-Park";
const char* WIFI_PASSWORD = "diorama123";

// Fixed IP so the master always knows where to reach this slave
const IPAddress SLAVE_IP    (192, 168, 4, 200);
const IPAddress GATEWAY_IP  (192, 168, 4,   1);
const IPAddress SUBNET_MASK (255, 255, 255,  0);

// ── RGB LED pins ──────────────────────────────────────────────────────────────
// Group 1: Left Fountain
#define G1_R  4
#define G1_G  5
#define G1_B  6
// Group 2: Right Fountain
#define G2_R  7
#define G2_G  22
#define G2_B  25
// Group 3: Inner Circle
#define G3_R  26
#define G3_G  27
#define G3_B  8

// ── State ─────────────────────────────────────────────────────────────────────
struct GroupState { uint8_t r, g, b, brightness; bool on; };
GroupState groups[3] = {
  {255, 180, 90, 100, true},
  {255, 180, 90, 100, true},
  {255, 180, 90, 100, true},
};

WebServer server(80);
unsigned long lastReconnectMs = 0;
bool wasConnected = false;

// ── PWM helpers ───────────────────────────────────────────────────────────────
// esp32 core 3.x uses analogWrite() directly — no ledcSetup/ledcAttachPin needed.
// Inverted drive: duty = 255 - (color * brightness / 100)

void writeRGB(uint8_t pinR, uint8_t pinG, uint8_t pinB,
              uint8_t r, uint8_t g, uint8_t b, uint8_t br) {
  analogWrite(pinR, 255 - (uint8_t)((uint32_t)r * br / 100));
  analogWrite(pinG, 255 - (uint8_t)((uint32_t)g * br / 100));
  analogWrite(pinB, 255 - (uint8_t)((uint32_t)b * br / 100));
}

void pushGroup(int idx) {
  const GroupState& g = groups[idx];
  uint8_t br = g.on ? g.brightness : 0;
  switch (idx) {
    case 0: writeRGB(G1_R, G1_G, G1_B, g.r, g.g, g.b, br); break;
    case 1: writeRGB(G2_R, G2_G, G2_B, g.r, g.g, g.b, br); break;
    case 2: writeRGB(G3_R, G3_G, G3_B, g.r, g.g, g.b, br); break;
  }
}

void pushAllGroups() { for (int i = 0; i < 3; i++) pushGroup(i); }

void allOff() {
  for (int i = 0; i < 3; i++) groups[i].on = false;
  pushAllGroups();
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────
void addCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
}

// GET /rgb?group=1|2|3|all&r=0-255&g=0-255&b=0-255&brightness=0-100
void handleRGB() {
  addCORS();
  if (!server.hasArg("group")) {
    server.send(400, "application/json", "{\"error\":\"missing group\"}");
    return;
  }

  uint8_t r  = server.hasArg("r")          ? constrain(server.arg("r").toInt(),          0, 255) : 255;
  uint8_t g  = server.hasArg("g")          ? constrain(server.arg("g").toInt(),          0, 255) : 180;
  uint8_t b  = server.hasArg("b")          ? constrain(server.arg("b").toInt(),          0, 255) : 90;
  uint8_t br = server.hasArg("brightness") ? constrain(server.arg("brightness").toInt(), 0, 100) : 100;

  String grp = server.arg("group");
  grp.toLowerCase();

  if (grp == "all") {
    for (int i = 0; i < 3; i++) { groups[i] = {r, g, b, br, true}; pushGroup(i); }
    Serial.printf("[RGB] ALL  #%02X%02X%02X br=%d\n", r, g, b, br);
  } else {
    int idx = grp.toInt() - 1;
    if (idx < 0 || idx > 2) {
      server.send(400, "application/json", "{\"error\":\"group must be 1,2,3 or all\"}");
      return;
    }
    groups[idx] = {r, g, b, br, true};
    pushGroup(idx);
    Serial.printf("[RGB] G%d  #%02X%02X%02X br=%d\n", idx + 1, r, g, b, br);
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

// GET /off
void handleOff() {
  addCORS();
  allOff();
  Serial.println("[RGB] All OFF");
  server.send(200, "application/json", "{\"ok\":true}");
}

// GET /status
void handleStatus() {
  addCORS();
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"connected\":true,\"ip\":\"%s\","
    "\"groups\":["
    "{\"id\":1,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"on\":%s},"
    "{\"id\":2,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"on\":%s},"
    "{\"id\":3,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"on\":%s}"
    "]}",
    WiFi.localIP().toString().c_str(),
    groups[0].r, groups[0].g, groups[0].b, groups[0].brightness, groups[0].on ? "true" : "false",
    groups[1].r, groups[1].g, groups[1].b, groups[1].brightness, groups[1].on ? "true" : "false",
    groups[2].r, groups[2].g, groups[2].b, groups[2].brightness, groups[2].on ? "true" : "false"
  );
  server.send(200, "application/json", buf);
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[SLAVE] Diorama RGB Slave starting...");

  // Set all LED pins as output and start fully OFF (inverted: 255 = off)
  uint8_t pins[] = {G1_R, G1_G, G1_B, G2_R, G2_G, G2_B, G3_R, G3_G, G3_B};
  for (uint8_t p : pins) {
    pinMode(p, OUTPUT);
    analogWrite(p, 255);  // off
  }
  Serial.println("[PWM] 9 pins ready (inverted MOSFET, all OFF)");

  // Register HTTP routes
  server.on("/",       HTTP_GET,     []() { addCORS(); server.send(200, "text/plain", "Diorama Slave RGB Controller"); });
  server.on("/rgb",    HTTP_GET,     handleRGB);
  server.on("/off",    HTTP_GET,     handleOff);
  server.on("/status", HTTP_GET,     handleStatus);
  server.on("/rgb",    HTTP_OPTIONS, []() { addCORS(); server.send(204); });
  server.on("/off",    HTTP_OPTIONS, []() { addCORS(); server.send(204); });
  server.on("/status", HTTP_OPTIONS, []() { addCORS(); server.send(204); });
  server.begin();
  Serial.println("[HTTP] Slave server started on port 80");

  // Connect to master AP with static IP
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (!WiFi.config(SLAVE_IP, GATEWAY_IP, SUBNET_MASK))
    Serial.println("[WiFi] Static IP config failed");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to %s ...\n", WIFI_SSID);
  lastReconnectMs = millis();
}

void loop() {
  server.handleClient();

  bool connected = (WiFi.status() == WL_CONNECTED);
  unsigned long now = millis();

  if (connected && !wasConnected) {
    Serial.printf("[WiFi] Connected  IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    pushAllGroups();  // apply initial state
  }

  if (!connected && (now - lastReconnectMs >= 10000)) {
    Serial.println("[WiFi] Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastReconnectMs = now;
  }

  wasConnected = connected;
}
