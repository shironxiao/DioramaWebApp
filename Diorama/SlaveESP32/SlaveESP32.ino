/*
  Diorama Slave ESP32 — RGB Light & Inner Circle Motor Controller
  ─────────────────────────────────────────────────────────────────────────────
  Connects to the master ESP32 AP (Diorama-Park) as a WiFi station.
  Master sends mode + RGB + sound detection state; this board drives:
  1. MOSFET PWM outputs for 3 RGB zones (Left, Right, Inner Circle)
  2. ULN2003 stepper driver (IN1-IN4) for Inner Circle rotation on sound detection

  ── IMPORTANT (classic ESP32) ────────────────────────────────────────────────
  Do NOT use GPIO 6–11. They are wired to internal SPI flash.

  ── Safe wiring ──────────────────────────────────────────────────────────────
  Group 1  Left Fountain  : R=GPIO4,  G=GPIO5,  B=GPIO18
  Group 2  Right Fountain : R=GPIO19, G=GPIO22, B=GPIO32
  Group 3  Inner Circle   : R=GPIO33, G=GPIO27, B=GPIO23

  ── PAM8403 analog audio input ───────────────────────────────────────────────
  DAC2=GPIO25 -> Right channel input, DAC1=GPIO26 -> Left channel input

  ── Motor Driver (ULN2003 + 28BYJ-48) ────────────────────────────────────────
  Stepper inputs: IN1=GPIO13, IN2=GPIO14, IN3=GPIO16, IN4=GPIO17

  ── API (master calls these) ─────────────────────────────────────────────────
  GET /rgb?group=1|2|3|all&r=&g=&b=&brightness=&mode=Basic|Colorful|Sound|Adaptive&sound=0|1
  GET /off?mode=...
  GET /motor?state=on|off&dir=fwd|rev
  GET /status
*/

#include <WiFi.h>
#include <WebServer.h>

// ── WiFi (must match main ESP SoftAP) ─────────────────────────────────────────
const char* WIFI_SSID     = "Diorama-Park";
const char* WIFI_PASSWORD = "diorama123";

const IPAddress SLAVE_IP    (192, 168, 4, 200);
const IPAddress GATEWAY_IP  (192, 168, 4,   1);
const IPAddress SUBNET_MASK (255, 255, 255,  0);
const IPAddress DNS_IP      (192, 168, 4,   1);

// ── RGB LED pins — SAFE GPIOs only (never 6–11 on classic ESP32) ──────────────
// Group 1: Left Fountain
#define G1_R  4
#define G1_G  5
#define G1_B  18
// Group 2: Right Fountain
#define G2_R  19
#define G2_G  22
#define G2_B  32
// Group 3: Inner Circle
#define G3_R  33
#define G3_G  27
#define G3_B  23

// ── ULN2003 / 28BYJ-48 stepper input pins ─────────────────────────────────────
constexpr uint8_t IN1_PIN = 13;
constexpr uint8_t IN2_PIN = 14;
constexpr uint8_t IN3_PIN = 16;
constexpr uint8_t IN4_PIN = 17;

bool innerCircleMotorRunning = false;
bool manualMotorRunning = false;
bool motorForward = true;
uint8_t motorStepIndex = 0;
unsigned long lastMotorStepMs = 0;
constexpr unsigned long MOTOR_STEP_INTERVAL_MS = 2;
constexpr uint8_t MOTOR_PHASES[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1},
};
unsigned long lastSoundDetectedMs = 0;
constexpr unsigned long SOUND_MOTOR_HOLD_MS = 600; // Hold motor briefly across audio pulses for smooth rotation

struct GroupState { uint8_t r, g, b, brightness; bool on; };
GroupState groups[3] = {
  {255, 255, 255, 100, false},
  {255, 255, 255, 100, false},
  {255, 255, 255, 100, false},
};

String lightMode = "Basic";
WebServer server(80);

unsigned long lastReconnectMs = 0;
unsigned long lastStatusMs    = 0;
bool wasConnected = false;
bool httpStarted  = false;

// ── Motor Control Functions ───────────────────────────────────────────────────
void writeMotorPhase() {
  digitalWrite(IN1_PIN, MOTOR_PHASES[motorStepIndex][0] ? HIGH : LOW);
  digitalWrite(IN2_PIN, MOTOR_PHASES[motorStepIndex][1] ? HIGH : LOW);
  digitalWrite(IN3_PIN, MOTOR_PHASES[motorStepIndex][2] ? HIGH : LOW);
  digitalWrite(IN4_PIN, MOTOR_PHASES[motorStepIndex][3] ? HIGH : LOW);
}

void stopMotors() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
  innerCircleMotorRunning = false;
  manualMotorRunning = false;
}

void runInnerCircle(bool forward = true) {
  motorForward = forward;
  if (!innerCircleMotorRunning) {
    writeMotorPhase();
    lastMotorStepMs = millis();
  }
  innerCircleMotorRunning = true;
}

void serviceInnerCircleMotor(unsigned long now) {
  if (!innerCircleMotorRunning || now - lastMotorStepMs < MOTOR_STEP_INTERVAL_MS) return;

  motorStepIndex = motorForward
    ? (motorStepIndex + 1) % 8
    : (motorStepIndex + 7) % 8;
  writeMotorPhase();
  lastMotorStepMs = now;
}

void resolveRgbForMode(uint8_t& r, uint8_t& g, uint8_t& b) {
  if (lightMode == "Basic") {
    r = 255;
    g = 255;
    b = 255;
  }
}

void setModeFromArg() {
  if (!server.hasArg("mode")) return;
  String m = server.arg("mode");
  m.trim();
  if (m.equalsIgnoreCase("Basic"))          lightMode = "Basic";
  else if (m.equalsIgnoreCase("Colorful"))  lightMode = "Colorful";
  else if (m.equalsIgnoreCase("Sound"))     lightMode = "Sound";
  else if (m.equalsIgnoreCase("Adaptive"))  lightMode = "Adaptive";

  if (lightMode != "Sound") {
    lastSoundDetectedMs = 0;
    if (innerCircleMotorRunning && !manualMotorRunning) stopMotors();
  }
}

void writeRGB(uint8_t pinR, uint8_t pinG, uint8_t pinB,
              uint8_t r, uint8_t g, uint8_t b, uint8_t br) {
  // Inverted MOSFET: 0 = full on, 255 = off
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

void pushAllGroups() {
  for (int i = 0; i < 3; i++) pushGroup(i);
}

void allOff() {
  for (int i = 0; i < 3; i++) groups[i].on = false;
  pushAllGroups();
}

void addCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
}

void handleRGB() {
  addCORS();
  if (!server.hasArg("group")) {
    server.send(400, "application/json", "{\"error\":\"missing group\"}");
    return;
  }

  setModeFromArg();

  uint8_t r  = server.hasArg("r")          ? constrain(server.arg("r").toInt(),          0, 255) : 255;
  uint8_t g  = server.hasArg("g")          ? constrain(server.arg("g").toInt(),          0, 255) : 255;
  uint8_t b  = server.hasArg("b")          ? constrain(server.arg("b").toInt(),          0, 255) : 255;
  uint8_t br = server.hasArg("brightness") ? constrain(server.arg("brightness").toInt(), 0, 100) : 100;

  resolveRgbForMode(r, g, b);

  bool soundParam = false;
  if (server.hasArg("sound")) {
    String s = server.arg("sound");
    soundParam = (s == "1" || s.equalsIgnoreCase("true") || s.equalsIgnoreCase("on"));
  }
  if (lightMode == "Sound" && soundParam) {
    lastSoundDetectedMs = millis();
  }

  String grp = server.arg("group");
  grp.toLowerCase();

  if (grp == "all") {
    for (int i = 0; i < 3; i++) {
      groups[i] = {r, g, b, br, true};
      pushGroup(i);
    }
    Serial.printf("[RGB] mode=%s ALL #%02X%02X%02X br=%d sound=%d\n",
                  lightMode.c_str(), r, g, b, br, soundParam ? 1 : 0);
  } else {
    int idx = grp.toInt() - 1;
    if (idx < 0 || idx > 2) {
      server.send(400, "application/json", "{\"error\":\"group must be 1,2,3 or all\"}");
      return;
    }
    groups[idx] = {r, g, b, br, true};
    pushGroup(idx);
    Serial.printf("[RGB] mode=%s G%d #%02X%02X%02X br=%d sound=%d\n",
                  lightMode.c_str(), idx + 1, r, g, b, br, soundParam ? 1 : 0);
  }

  char ok[160];
  snprintf(ok, sizeof(ok),
           "{\"ok\":true,\"mode\":\"%s\",\"r\":%d,\"g\":%d,\"b\":%d,\"ip\":\"%s\",\"motor\":%s}",
           lightMode.c_str(), r, g, b, WiFi.localIP().toString().c_str(),
           innerCircleMotorRunning ? "true" : "false");
  server.send(200, "application/json", ok);
}

void handleOff() {
  addCORS();
  setModeFromArg();
  allOff();
  stopMotors();
  lastSoundDetectedMs = 0;
  Serial.printf("[RGB] mode=%s All OFF\n", lightMode.c_str());
  server.send(200, "application/json", "{\"ok\":true,\"motor\":false}");
}

void handleMotor() {
  addCORS();
  if (server.hasArg("state")) {
    String st = server.arg("state");
    bool fwd = server.hasArg("dir") ? (!server.arg("dir").equalsIgnoreCase("rev")) : true;
    if (st.equalsIgnoreCase("on") || st.equalsIgnoreCase("run") || st == "1") {
      manualMotorRunning = true;
      runInnerCircle(fwd);
      Serial.println("[MOTOR] Manual RUN");
    } else {
      stopMotors();
      lastSoundDetectedMs = 0;
      Serial.println("[MOTOR] Manual STOP");
    }
  }
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"ok\":true,\"motorRunning\":%s}", innerCircleMotorRunning ? "true" : "false");
  server.send(200, "application/json", buf);
}

void handleStatus() {
  addCORS();
  char buf[720];
  snprintf(buf, sizeof(buf),
    "{\"connected\":%s,\"ip\":\"%s\",\"mode\":\"%s\",\"ssid\":\"%s\","
    "\"motorRunning\":%s,"
    "\"groups\":["
    "{\"id\":1,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"on\":%s},"
    "{\"id\":2,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"on\":%s},"
    "{\"id\":3,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"on\":%s}"
    "]}",
    WiFi.status() == WL_CONNECTED ? "true" : "false",
    WiFi.localIP().toString().c_str(),
    lightMode.c_str(),
    WIFI_SSID,
    innerCircleMotorRunning ? "true" : "false",
    groups[0].r, groups[0].g, groups[0].b, groups[0].brightness, groups[0].on ? "true" : "false",
    groups[1].r, groups[1].g, groups[1].b, groups[1].brightness, groups[1].on ? "true" : "false",
    groups[2].r, groups[2].g, groups[2].b, groups[2].brightness, groups[2].on ? "true" : "false"
  );
  server.send(200, "application/json", buf);
}

void startHttpServer() {
  if (httpStarted) return;
  server.on("/",       HTTP_GET,     []() {
    addCORS();
    server.send(200, "text/plain", "Diorama Slave RGB & Motor Controller");
  });
  server.on("/rgb",    HTTP_GET,     handleRGB);
  server.on("/off",    HTTP_GET,     handleOff);
  server.on("/motor",  HTTP_GET,     handleMotor);
  server.on("/status", HTTP_GET,     handleStatus);
  server.on("/rgb",    HTTP_OPTIONS, []() { addCORS(); server.send(204); });
  server.on("/off",    HTTP_OPTIONS, []() { addCORS(); server.send(204); });
  server.on("/motor",  HTTP_OPTIONS, []() { addCORS(); server.send(204); });
  server.on("/status", HTTP_OPTIONS, []() { addCORS(); server.send(204); });
  server.begin();
  httpStarted = true;
  Serial.printf("[HTTP] Listening on http://%s/status\n", WiFi.localIP().toString().c_str());
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // keep radio awake for SoftAP reliability
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  // Static IP so master always finds us at 192.168.4.200
  if (!WiFi.config(SLAVE_IP, GATEWAY_IP, SUBNET_MASK, DNS_IP)) {
    Serial.println("[WiFi] Static IP config FAILED");
  } else {
    Serial.println("[WiFi] Static IP 192.168.4.200 configured");
  }

  WiFi.disconnect(false, true);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to \"%s\" ...\n", WIFI_SSID);
  lastReconnectMs = millis();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[SLAVE] Diorama RGB & Inner Circle Motor Slave starting...");
  Serial.println("[SLAVE] RGB pins: G1=4/5/18  G2=19/22/32  G3=33/27/23");
  Serial.println("[AUDIO] GPIO25/26 freed for PAM8403 analog inputs; playback not implemented");
  Serial.println("[SLAVE] Motor pins: IN1=13, IN2=14, IN3=16, IN4=17");

  // Init PWM pins OFF (inverted)
  const uint8_t pins[] = {G1_R, G1_G, G1_B, G2_R, G2_G, G2_B, G3_R, G3_G, G3_B};
  for (uint8_t p : pins) {
    pinMode(p, OUTPUT);
    analogWrite(p, 255);  // off
  }
  Serial.println("[PWM] 9 safe pins ready, all OFF");

  // Init Motor Pins OFF
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  stopMotors();
  Serial.println("[MOTOR] ULN2003 stepper outputs ready, all OFF");

  connectWifi();
}

void loop() {
  if (httpStarted) server.handleClient();

  const bool connected = (WiFi.status() == WL_CONNECTED);
  const unsigned long now = millis();

  // ── Inner circle sound-reactive rotation ─────────────────────────
  if (!manualMotorRunning && lightMode == "Sound" &&
      lastSoundDetectedMs != 0 && now - lastSoundDetectedMs < SOUND_MOTOR_HOLD_MS) {
    if (!innerCircleMotorRunning) {
      runInnerCircle(true);
      Serial.println("[MOTOR] Sound detected -> Inner Circle rotating");
    }
  } else if (!manualMotorRunning && innerCircleMotorRunning) {
    if (lightMode != "Sound" || lastSoundDetectedMs == 0 ||
        now - lastSoundDetectedMs >= SOUND_MOTOR_HOLD_MS) {
      stopMotors();
      Serial.println("[MOTOR] Sound paused -> Inner Circle stopped");
    }
  }
  serviceInnerCircleMotor(now);

  if (connected && !wasConnected) {
    Serial.printf("[WiFi] Connected  IP=%s  gateway=%s  RSSI=%d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.RSSI());

    if (WiFi.localIP() != SLAVE_IP) {
      Serial.printf("[WiFi] WARNING: got %s, expected 192.168.4.200 — master may not find me\n",
                    WiFi.localIP().toString().c_str());
    }

    startHttpServer();
    allOff();
    stopMotors();
    Serial.println("[SLAVE] Ready — waiting for master /rgb commands");
  }

  if (!connected) {
    if (wasConnected) {
      Serial.println("[WiFi] Lost connection to Diorama-Park");
      stopMotors();
    }
    // Reconnect every 5s (not disconnect/begin every loop)
    if (now - lastReconnectMs >= 5000) {
      Serial.println("[WiFi] Retry connect...");
      WiFi.disconnect(false, false);
      delay(50);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastReconnectMs = now;
    }
  } else if (now - lastStatusMs >= 15000) {
    lastStatusMs = now;
    Serial.printf("[SLAVE] OK ip=%s mode=%s motor=%s rssi=%d\n",
                  WiFi.localIP().toString().c_str(),
                  lightMode.c_str(),
                  innerCircleMotorRunning ? "ON" : "OFF",
                  WiFi.RSSI());
  }

  wasConnected = connected;
}
