/*
  Silvestre del Moro Park — Diorama Controller v4
  ESP32 + ILI9488 480×320 + XPT2046 touch + SD audio (I2S) + WiFi HTTP server

  TFT scope  : Gate access screen  |  Lights (Basic / Sound / Adaptive)  |  Audio
  Web App    : Full control via HTTP GET endpoints (same ESP32 state)

  ── Wiring ──────────────────────────────────────────────────────────────────
  TFT / touch : per User_Setup.h  (T_CLK 18, T_DIN 23, T_DO 19, T_CS 21)
  SD card     : SCK 14, MISO 33, MOSI 25, CS 13   (HSPI, separate from TFT)
  I2S amp     : BCLK 26, LRC 27, DIN 22           (e.g. MAX98357A)
  Mic         : GPIO 34  (ADC1 analog)
  RGB LEDs    : R=13, G=14, B=25  (PWM — shared with SD; disable SD when using)
  Fountain    : GPIO 33            (shared with SD MISO)
  Fingerprint : RX2=16 ← sensor TX,  TX2=17 → sensor RX  (UART2)
  Gate servo  : GPIO 15

  ── HTTP API (same endpoints as Web App) ────────────────────────────────────
  GET /api/light?state=on|off&brightness=0-100
  GET /api/mode?mode=Basic|Colorful|Sound+Reactive|Color+Adaptive
  GET /api/color?r=0-255&g=0-255&b=0-255&target=left|right|center|all
  GET /api/fountain?state=on|off&strength=0-100&auxStrength=0-100
  GET /api/gate?state=open|closed
  GET /api/gate/status                          → {"open":true|false}
  GET /api/audio/play?file=xxx.mp3
  GET /api/audio/pause
  GET /api/audio/stop
  GET /api/audio/volume?volume=0-100
  GET /api/audio/files                          → {"files":["001.mp3",...]}
  GET /api/sound-reactive?state=on|off&intensity=0-100
  GET /api/sound                                → {"detected":bool,"level":0-1023}
*/

// ── Types — defined inline to avoid header include issues ───────────────────
enum Page     { P_LIGHTS = 0, P_AUDIO = 1 };
enum Mode     { M_BASIC = 0, M_COLOR = 1, M_SOUND = 2, M_ADAPT = 3 };
enum Drag     { D_NONE = 0, D_BR, D_HUE, D_SAT, D_SENS, D_VOL };
enum GateState { GS_WAITING = 0, GS_SCANNING = 1, GS_OPEN = 2, GS_GOODBYE = 3 };

// ── Libraries ────────────────────────────────────────────────────────────────
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <SPIFFS.h>
#include "Audio.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ── WiFi credentials — change to your network ────────────────────────────────
#define WIFI_SSID  "YourSSID"
#define WIFI_PASS  "YourPassword"

// ── Pins ─────────────────────────────────────────────────────────────────────
#define SD_SCK    14
#define SD_MISO   33
#define SD_MOSI   25
#define SD_CS     13
#define I2S_BCLK  26
#define I2S_LRC   27
#define I2S_DOUT  22
#define MIC_PIN   34

#define RED_PIN      13
#define GREEN_PIN    14
#define BLUE_PIN     25
#define FOUNTAIN_PIN 33   // shared with SD MISO — see note above
#define GATE_PIN     15   // gate servo / relay

// Calibration
#define CALIBRATION_FILE "/DioramaCalData"
#define REPEAT_CAL       false

// ── Objects ───────────────────────────────────────────────────────────────────
TFT_eSPI   tft = TFT_eSPI();
SPIClass   sdSPI(HSPI);
Audio      audio;
WebServer  server(80);

// ── Colour palette (RGB565) ───────────────────────────────────────────────────
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
const uint16_t C_BG     = rgb565(245, 250, 247);
const uint16_t C_CARD   = rgb565(255, 255, 255);
const uint16_t C_HDR    = rgb565(255, 255, 255);
const uint16_t C_BORDER = rgb565(196, 217, 205);
const uint16_t C_TRACK  = rgb565(215, 228, 222);
const uint16_t C_TEXT   = rgb565( 20,  35,  30);
const uint16_t C_DIM    = rgb565(120, 140, 132);
const uint16_t C_WHITE  = rgb565(255, 255, 255);
const uint16_t C_GREEN  = rgb565( 16, 150,  95);
const uint16_t C_AMBER  = rgb565(230, 130,   0);
const uint16_t C_PURPLE = rgb565(124,  58, 237);
const uint16_t C_RED    = rgb565(220,  50,  60);

// ── Geometry ──────────────────────────────────────────────────────────────────
//  480 × 320 landscape.  Header = top 36px.  Usable area = y 36..319 (284px).
const int W = 480, H = 320;
const int HDR_H = 36;         // header bar height
const int BODY_Y = HDR_H;     // body starts here
const int BODY_H = H - HDR_H; // 284 px

// Slider rail X extents
const int SL_X0 = 20, SL_X1 = 460;

// ── Colour helpers ────────────────────────────────────────────────────────────
struct RGB { uint8_t r, g, b; };

RGB hsv(int h, int s, int v) {
  float S = s / 100.f, V = v / 100.f;
  float C = V * S, X = C * (1 - fabsf(fmodf(h / 60.f, 2.f) - 1)), m = V - C;
  float r, g, b;
  switch ((h / 60) % 6) {
    case 0: r = C; g = X; b = 0; break;
    case 1: r = X; g = C; b = 0; break;
    case 2: r = 0; g = C; b = X; break;
    case 3: r = 0; g = X; b = C; break;
    case 4: r = X; g = 0; b = C; break;
    default: r = C; g = 0; b = X;
  }
  return { (uint8_t)((r + m) * 255 + .5f),
           (uint8_t)((g + m) * 255 + .5f),
           (uint8_t)((b + m) * 255 + .5f) };
}
uint16_t c565(RGB c) { return tft.color565(c.r, c.g, c.b); }

// Preset palette (12 swatches on Colorful mode)
const RGB PAL[12] = {
  {255, 68, 68},{255,140,  0},{255,215,  0},{124,252,  0},
  {  0,191,255},{138, 43,226},{255,105,180},{255,255,255},
  {  0,206,209},{255, 99, 71},{ 65,105,225},{ 50,205, 50}
};

// ── Shared diorama state ──────────────────────────────────────────────────────
// Lights
bool    lightsOn    = false;   // start OFF until gate opens
int     brightness  = 75;      // 0-100
String  lightMode   = "Basic"; // Basic | Colorful | Sound Reactive | Color Adaptive
bool    soundReactive = false;
int     soundIntensity = 65;   // 0-100

// Color zones (left, right, center)  — index 0=left 1=right 2=center
RGB     zone[3]     = { {255,180,90}, {255,180,90}, {255,180,90} };
// Working colour for Colorful mode picker
int     hue = 36, sat = 46;
RGB     cur = {255, 180, 90};
int     lastApplied = -1; // 0=ALL 1=Left 2=Right 3=Center

// Fountain
bool    fountainOn  = false;
int     fountainStr = 100;  // 0-100
int     fountainAux = 75;   // 0-100

// Gate
bool    gateOpen    = false;
GateState gateState = GS_WAITING;
uint32_t  gateMs    = 0;
const uint32_t SCAN_MS    = 2000;
const uint32_t GOODBYE_MS = 3000;

// Audio
#define MAX_TRACKS 24
char     trackPath[MAX_TRACKS][48];
int      trackCount = 0, curTrack = 0;
int      volume     = 60;   // 0-100
bool     sdOk = false, audioPlaying = false, audioStarted = false;
uint32_t trackStartMs = 0;

// TFT page / mode
Page  page      = P_LIGHTS;
Mode  tftMode   = M_BASIC;   // TFT-local mode tab
Drag  dragging  = D_NONE;

// Sound reactive live level (mic)
int   sens    = 60;
float lvl     = 0;
int   hueBase = 0;
RGB   live    = {255, 180, 90};

bool wasTouched = false;

// Forward declarations
void drawGateScreen();
void enterGateState(GateState s);
void drawHeader();
void drawLights();
void drawAudio();
void onDrag(int x);
void pushZones();
void applyHardwareLight();
void applyHardwareGate(bool open);

// ═══════════════════════════════════════════════════════════════════════════════
// HARDWARE OUTPUT
// ═══════════════════════════════════════════════════════════════════════════════

// Push zone colours to PWM pins (brightness-scaled).
// All three zones share one RGB strip on current wiring → use zone[2] (center).
void pushZones() {
  for (int i = 0; i < 3; i++) {
    zone[i].r = constrain(zone[i].r, 0, 255);
    zone[i].g = constrain(zone[i].g, 0, 255);
    zone[i].b = constrain(zone[i].b, 0, 255);
  }
  if (lightsOn) {
    analogWrite(RED_PIN,   zone[2].r * brightness / 100);
    analogWrite(GREEN_PIN, zone[2].g * brightness / 100);
    analogWrite(BLUE_PIN,  zone[2].b * brightness / 100);
  } else {
    analogWrite(RED_PIN,   0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN,  0);
  }
}

void setAll(RGB c) {
  zone[0] = zone[1] = zone[2] = c;
  pushZones();
}

void applyHardwareGate(bool open) {
  // Replace with servo / relay:
  // myServo.write(open ? 90 : 0);
  // digitalWrite(GATE_PIN, open ? HIGH : LOW);
  Serial.printf("[GATE] Actuator: %s\n", open ? "OPEN" : "CLOSED");
}

// ═══════════════════════════════════════════════════════════════════════════════
// DRAWING HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// Draw text with given font, colours and datum
void txt(const char* s, int x, int y, int font, uint16_t fg, uint16_t bg,
         uint8_t datum = TL_DATUM) {
  tft.setTextColor(fg, bg);
  tft.setTextDatum(datum);
  tft.drawString(s, x, y, font);
}

// Rounded-rect card outline
void card(int x, int y, int w, int h, uint16_t fill, uint16_t border) {
  tft.fillRoundRect(x, y, w, h, 8, fill);
  tft.drawRoundRect(x, y, w, h, 8, border);
}

// Labelled button (filled when active)
void btn(int x, int y, int w, int h, const char* label, bool active,
         uint16_t accent) {
  card(x, y, w, h, active ? accent : C_CARD, active ? accent : C_BORDER);
  txt(label, x + w / 2, y + h / 2, 2,
      active ? C_WHITE : C_TEXT,
      active ? accent  : C_CARD,
      MC_DATUM);
}

// Toggle switch (44×22)
void toggleSw(int x, int y, bool on, uint16_t accent) {
  tft.fillRoundRect(x, y, 44, 22, 11, on ? accent : C_TRACK);
  tft.fillCircle(on ? x + 33 : x + 11, y + 11, 8, C_WHITE);
  tft.drawCircle(on ? x + 33 : x + 11, y + 11, 8, C_BORDER);
}

// Horizontal slider (rail + filled portion + thumb)
void drawSlider(int y, int val, uint16_t accent) {
  tft.fillRect(0, y - 18, W, 36, C_BG);
  tft.fillRoundRect(SL_X0, y - 4, SL_X1 - SL_X0, 8, 4, C_TRACK);
  int kx = SL_X0 + (long)(SL_X1 - SL_X0) * val / 100;
  if (kx > SL_X0)
    tft.fillRoundRect(SL_X0, y - 4, kx - SL_X0, 8, 4, accent);
  tft.fillCircle(kx, y, 12, accent);
  tft.drawCircle(kx, y, 12, C_TEXT);
}

// Gradient hue/sat slider
void drawGradSlider(int y, bool isHue) {
  tft.fillRect(0, y - 14, W, 28, C_BG);
  for (int x = SL_X0; x < SL_X1; x += 4) {
    int p = (long)(x - SL_X0) * 100 / (SL_X1 - SL_X0);
    RGB c = isHue ? hsv(p * 359 / 100, 100, 100) : hsv(hue, p, 100);
    tft.fillRect(x, y - 4, 4, 8, c565(c));
  }
  int kx = SL_X0 + (long)(SL_X1 - SL_X0) * (isHue ? hue * 100 / 359 : sat) / 100;
  tft.fillCircle(kx, y, 11, c565(isHue ? hsv(hue, 100, 100) : cur));
  tft.drawCircle(kx, y, 11, C_TEXT);
}

int pctFromX(int x) {
  return constrain((long)(x - SL_X0) * 100 / (SL_X1 - SL_X0), 0, 100);
}

void drawPct(int xRight, int y, int val, uint16_t col) {
  char b[8]; snprintf(b, 8, "%d%%", val);
  tft.setTextPadding(52);
  tft.setTextColor(col, C_BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(b, xRight, y, 4);
  tft.setTextPadding(0);
}

// Small label badge (used for section headings)
void label(const char* s, int x, int y) {
  txt(s, x, y, 2, C_DIM, C_BG, TL_DATUM);
}

// ═══════════════════════════════════════════════════════════════════════════════
// HEADER
// ═══════════════════════════════════════════════════════════════════════════════

void drawHeader() {
  tft.fillRect(0, 0, W, HDR_H, C_HDR);
  tft.drawFastHLine(0, HDR_H - 1, W, C_BORDER);

  // "Exit" tap zone — only shown when gate is open
  if (gateState == GS_OPEN) {
    tft.fillRect(0, 0, 60, HDR_H - 1, C_RED);
    txt("Exit", 30, HDR_H / 2, 2, C_WHITE, C_RED, MC_DATUM);
  }

  // Park name (centred between Exit and tab buttons)
  txt("Silvestre del Moro", 90, HDR_H / 2, 2, C_TEXT, C_HDR, ML_DATUM);

  // Page tabs
  bool aL = (page == P_LIGHTS), aA = (page == P_AUDIO);
  tft.fillRect(300, 0, 90, HDR_H - 1, aL ? C_AMBER  : C_HDR);
  txt("Lights", 345, HDR_H / 2, 2, aL ? C_WHITE : C_DIM, aL ? C_AMBER : C_HDR, MC_DATUM);
  tft.fillRect(390, 0, 90, HDR_H - 1, aA ? C_GREEN  : C_HDR);
  txt("Audio",  435, HDR_H / 2, 2, aA ? C_WHITE : C_DIM, aA ? C_GREEN : C_HDR, MC_DATUM);

  // WiFi / SD status dots (far right, top)
  tft.fillCircle(474, 8,  4, WiFi.status() == WL_CONNECTED ? C_GREEN : C_RED);
  tft.fillCircle(474, 22, 4, sdOk ? C_GREEN : C_RED);
}

// ═══════════════════════════════════════════════════════════════════════════════
// GATE SCREEN
// ═══════════════════════════════════════════════════════════════════════════════

void drawGateScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, 4, C_GREEN);

  txt("Silvestre del Moro Park", W / 2, 22, 2, C_DIM, C_BG, MC_DATUM);

  // Icon circle  (centre 240, 120; r=48)
  int cx = W / 2, cy = 110;
  tft.fillCircle(cx, cy, 48, C_CARD);
  tft.drawCircle(cx, cy, 48, C_GREEN);
  tft.drawCircle(cx, cy, 44, C_BORDER);

  switch (gateState) {
    case GS_WAITING: {
      // Fingerprint rings
      for (int r : {20, 13, 6}) tft.drawCircle(cx, cy, r, C_GREEN);
      tft.drawFastVLine(cx, cy - 20, 40, C_GREEN);
      tft.drawFastHLine(cx - 20, cy, 40, C_GREEN);

      txt("Hi! Welcome!", cx, 178, 4, C_TEXT, C_BG, MC_DATUM);
      txt("Please scan your fingerprint to enter.", cx, 208, 2, C_DIM, C_BG, MC_DATUM);

      // Scan button
      card(cx - 120, 240, 240, 50, C_GREEN, C_GREEN);
      txt("TAP HERE TO SCAN", cx, 265, 2, C_WHITE, C_GREEN, MC_DATUM);
      break;
    }
    case GS_SCANNING: {
      for (int r : {20, 13, 6}) tft.drawCircle(cx, cy, r, C_AMBER);
      tft.drawFastVLine(cx, cy - 20, 40, C_AMBER);
      tft.drawFastHLine(cx - 20, cy, 40, C_AMBER);

      txt("Scanning...", cx, 178, 4, C_AMBER, C_BG, MC_DATUM);
      txt("Verifying fingerprint, please wait...", cx, 208, 2, C_DIM, C_BG, MC_DATUM);

      // Progress bar rail
      tft.fillRoundRect(cx - 120, 240, 240, 12, 6, C_TRACK);
      uint32_t elapsed = millis() - gateMs;
      int fw = constrain((int)(240L * elapsed / SCAN_MS), 0, 240);
      if (fw > 0) tft.fillRoundRect(cx - 120, 240, fw, 12, 6, C_GREEN);
      break;
    }
    case GS_OPEN: {
      // Open padlock: arc (top half of circle) + body
      tft.drawArc(cx, cy - 6, 18, 12, 180, 360, C_GREEN, C_CARD);
      tft.fillRoundRect(cx - 18, cy + 6, 36, 28, 4, C_GREEN);
      tft.fillCircle(cx, cy + 17, 5, C_WHITE);

      txt("Welcome!", cx, 178, 4, C_GREEN, C_BG, MC_DATUM);
      txt("You may now interact with the diorama.", cx, 208, 2, C_DIM, C_BG, MC_DATUM);
      break;
    }
    case GS_GOODBYE: {
      // Closed padlock
      tft.drawArc(cx, cy - 6, 18, 12, 0, 360, C_RED, C_CARD);
      tft.fillRoundRect(cx - 18, cy + 6, 36, 28, 4, C_RED);
      tft.fillCircle(cx, cy + 17, 5, C_WHITE);

      txt("Goodbye!", cx, 178, 4, C_RED, C_BG, MC_DATUM);
      txt("Thank you for visiting.", cx, 208, 2, C_DIM, C_BG, MC_DATUM);
      break;
    }
  }
}

void enterGateState(GateState s) {
  gateState = s;
  gateMs    = millis();

  switch (s) {
    case GS_WAITING:
      Serial.println("[GATE] WAITING");
      gateOpen = false;
      lightsOn = false;
      applyHardwareGate(false);
      pushZones();
      drawGateScreen();
      break;

    case GS_SCANNING:
      Serial.println("[GATE] SCANNING");
      drawGateScreen();
      break;

    case GS_OPEN:
      Serial.println("[GATE] OPEN — UI unlocked");
      gateOpen = true;
      lightsOn = true;
      applyHardwareGate(true);
      pushZones();
      drawGateScreen();
      delay(1400);
      drawHeader();
      drawLights();
      break;

    case GS_GOODBYE:
      Serial.println("[GATE] GOODBYE");
      gateOpen = false;
      lightsOn = false;
      fountainOn = false;
      applyHardwareGate(false);
      pushZones();
      drawGateScreen();
      break;
  }
}

void pollGate() {
  if (gateState == GS_SCANNING) {
    // Animate the progress bar while waiting
    uint32_t elapsed = millis() - gateMs;
    if (elapsed >= SCAN_MS) {
      enterGateState(GS_OPEN);
    } else {
      int cx = W / 2;
      int fw = constrain((int)(240L * elapsed / SCAN_MS), 0, 240);
      tft.fillRoundRect(cx - 120, 240, fw, 12, 6, C_GREEN);
    }
  } else if (gateState == GS_GOODBYE) {
    if (millis() - gateMs >= GOODBYE_MS) enterGateState(GS_WAITING);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// LIGHTS PAGE
// ═══════════════════════════════════════════════════════════════════════════════
//
//  Layout (480×284 body, y origin = HDR_H=36):
//
//  y  36..78  : Mode tabs  [ Basic ][ Sound ][ Adaptive ]  (3 tabs, 150×38 each)
//  y  82..138 : Power card (on/off toggle + current mode label)
//  y 142..162 : "Brightness" label + value
//  y 163..199 : Brightness slider
//  y 206..226 : Mode-specific label
//  y 230..282 : Mode-specific content (level bar OR swatch)  / Sens slider
//
// ─────────────────────────────────────────────────────────────────────────────

// Mode tab row  (3 tabs: Basic, Sound, Adaptive — Colorful is web-only on TFT)
// Tab layout: each 150 px wide, gap 5 px, start x=10
// Total = 3×150 + 2×5 = 460 px  fits in 480
void drawModeTabs() {
  const char* labels[3] = { "Basic", "Sound", "Adaptive" };
  const Mode  modes[3]  = { M_BASIC, M_SOUND, M_ADAPT };
  for (int i = 0; i < 3; i++) {
    bool act = (tftMode == modes[i]);
    int  bx  = 10 + i * 157;
    btn(bx, BODY_Y + 2, 150, 36, labels[i], act, C_AMBER);
  }
}

// Power toggle card
void drawPowerCard() {
  int cy = BODY_Y + 44;
  card(10, cy, 460, 52, C_CARD, lightsOn ? C_AMBER : C_BORDER);

  // Icon circle
  tft.fillCircle(40, cy + 26, 16, lightsOn ? C_AMBER : C_TRACK);
  // Simple power symbol (circle + vertical line on top)
  tft.drawCircle(40, cy + 26, 10, lightsOn ? C_WHITE : C_DIM);
  tft.drawFastVLine(40, cy + 16, 10, lightsOn ? C_WHITE : C_DIM);

  // Label
  txt(lightsOn ? "Lights  ON" : "Lights  OFF",
      68, cy + 14, 4, C_TEXT, C_CARD, TL_DATUM);
  txt(lightMode.c_str(), 68, cy + 36, 2, C_DIM, C_CARD, TL_DATUM);

  // Toggle switch
  toggleSw(408, cy + 15, lightsOn, C_AMBER);
}

// Brightness row
void drawBrightnessRow() {
  int ly = BODY_Y + 108;
  tft.fillRect(0, ly, W, 70, C_BG);
  label("Brightness", SL_X0, ly);
  drawPct(W - 10, ly + 4, brightness, C_AMBER);
  drawSlider(ly + 36, brightness, C_AMBER);
}

// Mode-specific lower panel
void drawModePanel() {
  int py = BODY_Y + 184;
  tft.fillRect(0, py, W, H - py, C_BG);

  if (tftMode == M_BASIC) {
    // Nothing extra — brightness slider is sufficient
    label("Steady warm light. Adjust brightness above.", SL_X0, py + 4);
    return;
  }

  if (tftMode == M_SOUND) {
    label("Sound level", SL_X0, py);
    // Level bar
    int bw = (int)(lvl * (SL_X1 - SL_X0));
    tft.fillRect(SL_X0, py + 18, SL_X1 - SL_X0, 20, C_TRACK);
    if (bw > 0) tft.fillRect(SL_X0, py + 18, bw, 20, c565(live));
    // Sensitivity slider
    label("Sensitivity", SL_X0, py + 50);
    drawPct(W - 10, py + 54, sens, C_AMBER);
    drawSlider(py + 82, sens, C_AMBER);
    return;
  }

  if (tftMode == M_ADAPT) {
    label("Ambient colour sensor — auto-adapting", SL_X0, py + 4);
    // Colour swatch
    tft.fillRoundRect(SL_X0, py + 24, SL_X1 - SL_X0, 56, 8, c565(live));
    tft.drawRoundRect(SL_X0, py + 24, SL_X1 - SL_X0, 56, 8, C_BORDER);
    return;
  }
}

void drawLights() {
  tft.fillRect(0, BODY_Y, W, BODY_H, C_BG);
  drawModeTabs();
  drawPowerCard();
  drawBrightnessRow();
  drawModePanel();
}

// ═══════════════════════════════════════════════════════════════════════════════
// AUDIO PAGE
// ═══════════════════════════════════════════════════════════════════════════════
//
//  Layout (body y 36..319):
//
//  y  40..76  : Track title + counter
//  y  80..96  : Playback status
//  y 102..170 : Transport buttons  [  <<  ]  [ Play/Pause ]  [  >>  ]
//  y 178..198 : "Volume" label + value
//  y 202..240 : Volume slider
//
// ─────────────────────────────────────────────────────────────────────────────

void drawAudio() {
  tft.fillRect(0, BODY_Y, W, BODY_H, C_BG);

  // Track info
  if (!sdOk) {
    txt("SD card not found", W / 2, BODY_Y + 28, 4, C_RED, C_BG, MC_DATUM);
  } else if (!trackCount) {
    txt("No audio files on SD", W / 2, BODY_Y + 28, 4, C_AMBER, C_BG, MC_DATUM);
  } else {
    // Shorten filename: strip path and extension, truncate at 26 chars
    char title[48];
    strlcpy(title, trackPath[curTrack] + 1, sizeof(title));
    char* dot = strrchr(title, '.'); if (dot) *dot = 0;
    if (strlen(title) > 26) { title[24] = '.'; title[25] = '.'; title[26] = 0; }

    tft.setTextPadding(460);
    txt(title, W / 2, BODY_Y + 20, 4, C_TEXT, C_BG, MC_DATUM);
    tft.setTextPadding(0);

    char ctr[24];
    snprintf(ctr, sizeof(ctr), "Track %d / %d", curTrack + 1, trackCount);
    txt(ctr, W / 2, BODY_Y + 48, 2, C_DIM, C_BG, MC_DATUM);
  }

  // Playback status badge
  const char* statusStr = audioPlaying ? "▶  Playing"
                        : audioStarted ? "⏸  Paused"
                        : "⏹  Stopped";
  uint16_t statusCol = audioPlaying ? C_GREEN : C_DIM;
  txt(statusStr, W / 2, BODY_Y + 70, 2, statusCol, C_BG, MC_DATUM);

  // Transport buttons  (centred block)
  //  [<<]   starts x=30,  w=110
  //  [Play] starts x=185, w=110
  //  [>>]   starts x=340, w=110
  btn( 30, BODY_Y + 92, 110, 58, "<<",                   false,        C_GREEN);
  btn(185, BODY_Y + 92, 110, 58, audioPlaying ? "Pause" : "Play",
      audioPlaying, C_GREEN);
  btn(340, BODY_Y + 92, 110, 58, ">>",                   false,        C_GREEN);

  // Volume
  label("Volume", SL_X0, BODY_Y + 162);
  drawPct(W - 10, BODY_Y + 166, volume, C_GREEN);
  drawSlider(BODY_Y + 200, volume, C_GREEN);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SOUND SENSOR
// ═══════════════════════════════════════════════════════════════════════════════

float readMicLevel() {
  int mn = 4095, mx = 0;
  for (int i = 0; i < 80; i++) {
    int v = analogRead(MIC_PIN);
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  int range = 2000 - 18 * sens;
  if (range < 150) range = 150;
  return constrain((mx - mn - 60) / (float)range, 0.f, 1.f);
}

void updateLive() {
  static uint32_t last = 0;
  if (millis() - last < 40) return;
  last = millis();

  if (lightMode == "Sound Reactive" && lightsOn) {
    lvl     = max(readMicLevel(), lvl * 0.82f);
    hueBase = (hueBase + 2) % 360;
    live    = hsv((hueBase + (int)(lvl * 120)) % 360, 100, 12 + (int)(88 * lvl));
    setAll(live);
    // Refresh level bar if on lights page
    if (page == P_LIGHTS && tftMode == M_SOUND) {
      int py = BODY_Y + 184;
      int bw = (int)(lvl * (SL_X1 - SL_X0));
      tft.fillRect(SL_X0, py + 18, SL_X1 - SL_X0, 20, C_TRACK);
      if (bw > 0) tft.fillRect(SL_X0, py + 18, bw, 20, c565(live));
    }
  }

  // Colour Adaptive — stub (connect TCS34725 or similar)
  // if (lightMode == "Color Adaptive") { ... }
}

// ═══════════════════════════════════════════════════════════════════════════════
// AUDIO HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

int volToAudio(int v) { return map(v, 0, 100, 0, 21); }

void scanTracks() {
  trackCount = 0;
  File root = SD.open("/");
  if (!root) return;
  File f = root.openNextFile();
  while (f && trackCount < MAX_TRACKS) {
    if (!f.isDirectory()) {
      String n = f.name();
      if (n.startsWith("/")) n.remove(0, 1);
      String l = n; l.toLowerCase();
      if (n[0] != '.' && (l.endsWith(".mp3") || l.endsWith(".wav") ||
                          l.endsWith(".aac") || l.endsWith(".m4a") || l.endsWith(".flac"))) {
        snprintf(trackPath[trackCount++], 48, "/%s", n.c_str());
      }
    }
    f.close();
    f = root.openNextFile();
  }
  root.close();
  for (int i = 0; i < trackCount - 1; i++)
    for (int j = i + 1; j < trackCount; j++)
      if (strcasecmp(trackPath[i], trackPath[j]) > 0) {
        char t[48]; strcpy(t, trackPath[i]);
        strcpy(trackPath[i], trackPath[j]);
        strcpy(trackPath[j], t);
      }
}

void startTrack(int i) {
  if (!trackCount) return;
  curTrack = i;
  audio.connecttoFS(SD, trackPath[i]);
  audioPlaying = audioStarted = true;
  trackStartMs = millis();
  Serial.printf("[AUDIO] Playing %d/%d: %s\n", curTrack + 1, trackCount, trackPath[i]);
}

void togglePlay() {
  if (!trackCount) return;
  if (!audioStarted) { startTrack(curTrack); return; }
  audio.pauseResume();
  audioPlaying = !audioPlaying;
}

void stepTrack(int d) {
  if (!trackCount) return;
  int n = (curTrack + d + trackCount) % trackCount;
  if (audioStarted) startTrack(n); else curTrack = n;
}

void autoAdvance() {
  if (audioPlaying && audioStarted &&
      millis() - trackStartMs > 1000 && !audio.isRunning()) {
    stepTrack(1);
    if (page == P_AUDIO) drawAudio();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TOUCH — onPress & onDrag
// ═══════════════════════════════════════════════════════════════════════════════

void onPress(int x, int y) {
  // ── Gate screen ──────────────────────────────────────────────────────────
  if (gateState != GS_OPEN) {
    if (gateState == GS_WAITING && inRect(x, y, W/2 - 120, 240, 240, 50)) {
      enterGateState(GS_SCANNING);
    }
    return;
  }

  // ── Header taps ──────────────────────────────────────────────────────────
  if (y < HDR_H) {
    // Exit tap zone (left 60 px)
    if (x < 60) {
      enterGateState(GS_GOODBYE);
      return;
    }
    // Page tabs
    if (inRect(x, y, 300, 0, 90, HDR_H)) {
      if (page != P_LIGHTS) { page = P_LIGHTS; drawHeader(); drawLights(); }
      return;
    }
    if (inRect(x, y, 390, 0, 90, HDR_H)) {
      if (page != P_AUDIO) { page = P_AUDIO; drawHeader(); drawAudio(); }
      return;
    }
    return;
  }

  // ── Audio page ────────────────────────────────────────────────────────────
  if (page == P_AUDIO) {
    // <<
    if (inRect(x, y, 30,  BODY_Y + 92, 110, 58)) { stepTrack(-1); drawAudio(); return; }
    // Play/Pause
    if (inRect(x, y, 185, BODY_Y + 92, 110, 58)) { togglePlay();  drawAudio(); return; }
    // >>
    if (inRect(x, y, 340, BODY_Y + 92, 110, 58)) { stepTrack(1);  drawAudio(); return; }
    // Volume slider zone
    if (inRect(x, y, SL_X0, BODY_Y + 182, SL_X1 - SL_X0, 40)) {
      dragging = D_VOL; onDrag(x);
    }
    return;
  }

  // ── Lights page ───────────────────────────────────────────────────────────
  if (page == P_LIGHTS) {
    // Mode tabs row  y = BODY_Y+2 .. BODY_Y+38
    if (inRect(x, y, 0, BODY_Y, W, 40)) {
      const Mode modes[3] = { M_BASIC, M_SOUND, M_ADAPT };
      for (int i = 0; i < 3; i++) {
        if (inRect(x, y, 10 + i * 157, BODY_Y + 2, 150, 36)) {
          if (tftMode != modes[i]) {
            tftMode = modes[i];
            // Sync lightMode string with web app
            if (tftMode == M_BASIC)  lightMode = "Basic";
            if (tftMode == M_SOUND)  lightMode = "Sound Reactive";
            if (tftMode == M_ADAPT)  lightMode = "Color Adaptive";
            Serial.printf("[MODE] %s\n", lightMode.c_str());
            if (tftMode == M_BASIC) setAll({255, 180, 90});
            drawLights();
          }
          return;
        }
      }
    }

    // Power toggle card  y = BODY_Y+44 .. BODY_Y+96
    if (inRect(x, y, 10, BODY_Y + 44, 460, 52)) {
      lightsOn = !lightsOn;
      Serial.printf("[LIGHTS] %s\n", lightsOn ? "ON" : "OFF");
      pushZones();
      drawPowerCard();
      return;
    }

    // Brightness slider zone  y = BODY_Y+145 .. BODY_Y+185
    if (inRect(x, y, SL_X0, BODY_Y + 126, SL_X1 - SL_X0, 56)) {
      dragging = D_BR; onDrag(x); return;
    }

    // Sensitivity slider (Sound mode only)
    if (tftMode == M_SOUND) {
      int py = BODY_Y + 184;
      if (inRect(x, y, SL_X0, py + 62, SL_X1 - SL_X0, 56)) {
        dragging = D_SENS; onDrag(x); return;
      }
    }
  }
}

void onDrag(int x) {
  int p = pctFromX(x);
  switch (dragging) {
    case D_BR:
      if (p != brightness) {
        brightness = p;
        Serial.printf("[LIGHTS] Brightness %d%%\n", brightness);
        drawPct(W - 10, BODY_Y + 112, p, C_AMBER);
        drawSlider(BODY_Y + 144, p, C_AMBER);
        pushZones();
      }
      break;

    case D_SENS:
      if (p != sens) {
        sens = p;
        Serial.printf("[SOUND] Sensitivity %d%%\n", sens);
        int py = BODY_Y + 184;
        drawPct(W - 10, py + 54, p, C_AMBER);
        drawSlider(py + 82, p, C_AMBER);
      }
      break;

    case D_VOL:
      if (p != volume) {
        volume = p;
        Serial.printf("[AUDIO] Volume %d%%\n", volume);
        drawPct(W - 10, BODY_Y + 166, p, C_GREEN);
        drawSlider(BODY_Y + 200, p, C_GREEN);
        audio.setVolume(volToAudio(p));
      }
      break;

    default: break;
  }
}

void pollTouch() {
  static uint8_t miss = 0;
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    miss = 0;
    if (!wasTouched) { wasTouched = true; onPress(x, y); }
    else if (dragging) onDrag(x);
  } else if (wasTouched && ++miss >= 3) {
    wasTouched = false; miss = 0;
    dragging = D_NONE;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HTTP SERVER — endpoint handlers
// All endpoints match the web app's esp32Api.js exactly.
// ═══════════════════════════════════════════════════════════════════════════════

// Helper: send JSON OK
void jsonOk(const char* extra = "") {
  char buf[128];
  if (strlen(extra) == 0)
    snprintf(buf, sizeof(buf), "{\"ok\":true}");
  else
    snprintf(buf, sizeof(buf), "{\"ok\":true,%s}", extra);
  server.send(200, "application/json", buf);
}

// ── GET /api/light?state=on|off&brightness=0-100 ─────────────────────────────
void handleLight() {
  if (server.hasArg("state")) {
    lightsOn = (server.arg("state") == "on");
  }
  if (server.hasArg("brightness")) {
    brightness = constrain(server.arg("brightness").toInt(), 0, 100);
  }
  Serial.printf("[HTTP /api/light] state=%s brightness=%d\n",
                lightsOn ? "on" : "off", brightness);
  pushZones();
  if (page == P_LIGHTS && gateState == GS_OPEN) {
    drawPowerCard();
    drawBrightnessRow();
  }
  jsonOk();
}

// ── GET /api/mode?mode=Basic|Sound+Reactive|Color+Adaptive|Colorful ──────────
void handleMode() {
  if (server.hasArg("mode")) {
    lightMode = server.arg("mode");
    lightMode.replace("+", " ");  // URL-encoded spaces
    Serial.printf("[HTTP /api/mode] mode=%s\n", lightMode.c_str());
    // Sync TFT mode tab
    if (lightMode == "Basic")         tftMode = M_BASIC;
    else if (lightMode == "Sound Reactive")  tftMode = M_SOUND;
    else if (lightMode == "Color Adaptive")  tftMode = M_ADAPT;
    else                              tftMode = M_BASIC; // Colorful → show Basic on TFT
    if (tftMode == M_BASIC) setAll({255, 180, 90});
    if (page == P_LIGHTS && gateState == GS_OPEN) drawLights();
  }
  jsonOk();
}

// ── GET /api/color?r=&g=&b=&target=left|right|center|all ────────────────────
void handleColor() {
  uint8_t r = server.hasArg("r") ? constrain(server.arg("r").toInt(), 0, 255) : 255;
  uint8_t g = server.hasArg("g") ? constrain(server.arg("g").toInt(), 0, 255) : 180;
  uint8_t b = server.hasArg("b") ? constrain(server.arg("b").toInt(), 0, 255) : 90;
  String  target = server.hasArg("target") ? server.arg("target") : "all";

  Serial.printf("[HTTP /api/color] r=%d g=%d b=%d target=%s\n", r, g, b, target.c_str());

  RGB col = {r, g, b};
  if      (target == "left")   zone[0] = col;
  else if (target == "right")  zone[1] = col;
  else if (target == "center") zone[2] = col;
  else                         zone[0] = zone[1] = zone[2] = col;

  pushZones();
  jsonOk();
}

// ── GET /api/fountain?state=on|off&strength=0-100&auxStrength=0-100 ──────────
void handleFountain() {
  if (server.hasArg("state"))       fountainOn  = (server.arg("state") == "on");
  if (server.hasArg("strength"))    fountainStr = constrain(server.arg("strength").toInt(), 0, 100);
  if (server.hasArg("auxStrength")) fountainAux = constrain(server.arg("auxStrength").toInt(), 0, 100);

  Serial.printf("[HTTP /api/fountain] state=%s str=%d aux=%d\n",
                fountainOn ? "on" : "off", fountainStr, fountainAux);

  // Fountain GPIO — shared with SD MISO; only enable if SD is not active
  // analogWrite(FOUNTAIN_PIN, fountainOn ? map(fountainStr, 0, 100, 0, 255) : 0);
  jsonOk();
}

// ── GET /api/gate?state=open|closed ─────────────────────────────────────────
void handleGate() {
  bool wantOpen = server.hasArg("state") && server.arg("state") == "open";
  Serial.printf("[HTTP /api/gate] state=%s\n", wantOpen ? "open" : "closed");

  if (wantOpen && !gateOpen) {
    enterGateState(GS_OPEN);
  } else if (!wantOpen && gateOpen) {
    enterGateState(GS_GOODBYE);
  }
  jsonOk();
}

// ── GET /api/gate/status  → {"open":true|false} ──────────────────────────────
void handleGateStatus() {
  char buf[32];
  snprintf(buf, sizeof(buf), "{\"open\":%s}", gateOpen ? "true" : "false");
  server.send(200, "application/json", buf);
}

// ── GET /api/audio/play?file=xxx.mp3 ─────────────────────────────────────────
void handleAudioPlay() {
  if (!server.hasArg("file")) { server.send(400, "application/json", "{\"error\":\"missing file\"}"); return; }
  String file = "/" + server.arg("file");
  if (!file.startsWith("/")) file = "/" + file;

  Serial.printf("[HTTP /api/audio/play] file=%s\n", file.c_str());
  // Find track index matching filename
  for (int i = 0; i < trackCount; i++) {
    if (String(trackPath[i]) == file || String(trackPath[i]) == server.arg("file")) {
      startTrack(i);
      if (page == P_AUDIO && gateState == GS_OPEN) drawAudio();
      jsonOk();
      return;
    }
  }
  // File not in list — play directly
  audio.connecttoFS(SD, file.c_str());
  audioPlaying = audioStarted = true;
  trackStartMs = millis();
  if (page == P_AUDIO && gateState == GS_OPEN) drawAudio();
  jsonOk();
}

// ── GET /api/audio/pause ─────────────────────────────────────────────────────
void handleAudioPause() {
  Serial.println("[HTTP /api/audio/pause]");
  if (audioStarted && audioPlaying) {
    audio.pauseResume();
    audioPlaying = false;
    if (page == P_AUDIO && gateState == GS_OPEN) drawAudio();
  }
  jsonOk();
}

// ── GET /api/audio/stop ──────────────────────────────────────────────────────
void handleAudioStop() {
  Serial.println("[HTTP /api/audio/stop]");
  audio.stopSong();
  audioPlaying = audioStarted = false;
  if (page == P_AUDIO && gateState == GS_OPEN) drawAudio();
  jsonOk();
}

// ── GET /api/audio/volume?volume=0-100 ───────────────────────────────────────
void handleAudioVolume() {
  if (server.hasArg("volume")) {
    volume = constrain(server.arg("volume").toInt(), 0, 100);
    audio.setVolume(volToAudio(volume));
    Serial.printf("[HTTP /api/audio/volume] volume=%d\n", volume);
    if (page == P_AUDIO && gateState == GS_OPEN) {
      drawPct(W - 10, BODY_Y + 166, volume, C_GREEN);
      drawSlider(BODY_Y + 200, volume, C_GREEN);
    }
  }
  jsonOk();
}

// ── GET /api/audio/files  → {"files":["001.mp3",...]} ────────────────────────
void handleAudioFiles() {
  String json = "{\"files\":[";
  for (int i = 0; i < trackCount; i++) {
    if (i) json += ",";
    // Return filename without leading slash
    String p = String(trackPath[i]);
    if (p.startsWith("/")) p.remove(0, 1);
    json += "\"" + p + "\"";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ── GET /api/sound-reactive?state=on|off&intensity=0-100 ─────────────────────
void handleSoundReactive() {
  if (server.hasArg("state"))     soundReactive   = (server.arg("state") == "on");
  if (server.hasArg("intensity")) soundIntensity  = constrain(server.arg("intensity").toInt(), 0, 100);
  Serial.printf("[HTTP /api/sound-reactive] state=%s intensity=%d\n",
                soundReactive ? "on" : "off", soundIntensity);
  if (soundReactive) {
    lightMode = "Sound Reactive";
    tftMode   = M_SOUND;
    if (page == P_LIGHTS && gateState == GS_OPEN) drawLights();
  }
  jsonOk();
}

// ── GET /api/sound  → {"detected":bool,"level":0-1023} ───────────────────────
void handleSound() {
  // Take a quick ADC sample
  int mn = 4095, mx = 0;
  for (int i = 0; i < 20; i++) { int v = analogRead(MIC_PIN); if (v < mn) mn = v; if (v > mx) mx = v; }
  int level = mx - mn;
  bool detected = level > 80;
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"detected\":%s,\"level\":%d}", detected ? "true" : "false", level);
  server.send(200, "application/json", buf);
}

// ── GET /api/color-sensor ─────────────────────────────────────────────────────
void handleColorSensor() {
  // Stub — replace with real TCS34725 read when wired
  server.send(404, "application/json", "{\"error\":\"no sensor\"}");
}

// ── GET /api/state  → full diorama state snapshot ────────────────────────────
// The web app polls this every few seconds to sync changes made on the TFT.
void handleState() {
  // Build zone colour hex strings
  char leftHex[8], rightHex[8], centerHex[8];
  snprintf(leftHex,   sizeof(leftHex),   "#%02X%02X%02X", zone[0].r, zone[0].g, zone[0].b);
  snprintf(rightHex,  sizeof(rightHex),  "#%02X%02X%02X", zone[1].r, zone[1].g, zone[1].b);
  snprintf(centerHex, sizeof(centerHex), "#%02X%02X%02X", zone[2].r, zone[2].g, zone[2].b);

  // Current track name (strip leading slash and extension)
  char trackName[48] = "";
  if (trackCount > 0) {
    strlcpy(trackName, trackPath[curTrack] + 1, sizeof(trackName));
    char* dot = strrchr(trackName, '.'); if (dot) *dot = 0;
  }

  StaticJsonDocument<512> doc;
  doc["gateOpen"]        = gateOpen;
  doc["lightsOn"]        = lightsOn;
  doc["brightness"]      = brightness;
  doc["lightingMode"]    = lightMode;
  doc["soundReactive"]   = soundReactive;
  doc["soundIntensity"]  = soundIntensity;
  doc["fountainOn"]      = fountainOn;
  doc["fountainStr"]     = fountainStr;
  doc["fountainAux"]     = fountainAux;
  doc["volume"]          = volume;
  doc["audioPlaying"]    = audioPlaying;
  doc["audioTrack"]      = (trackCount > 0) ? trackName : "";
  doc["fountainColor"]   = leftHex;
  doc["fountainAuxColor"]= rightHex;
  doc["circleColor"]     = centerHex;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ── CORS preflight (OPTIONS) ──────────────────────────────────────────────────
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

// ── Attach CORS headers to every response ────────────────────────────────────
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void setupRoutes() {
  // CORS preflight
  server.on("/api/light",            HTTP_OPTIONS, handleOptions);
  server.on("/api/mode",             HTTP_OPTIONS, handleOptions);
  server.on("/api/color",            HTTP_OPTIONS, handleOptions);
  server.on("/api/fountain",         HTTP_OPTIONS, handleOptions);
  server.on("/api/gate",             HTTP_OPTIONS, handleOptions);
  server.on("/api/gate/status",      HTTP_OPTIONS, handleOptions);
  server.on("/api/audio/play",       HTTP_OPTIONS, handleOptions);
  server.on("/api/audio/pause",      HTTP_OPTIONS, handleOptions);
  server.on("/api/audio/stop",       HTTP_OPTIONS, handleOptions);
  server.on("/api/audio/volume",     HTTP_OPTIONS, handleOptions);
  server.on("/api/audio/files",      HTTP_OPTIONS, handleOptions);
  server.on("/api/sound-reactive",   HTTP_OPTIONS, handleOptions);
  server.on("/api/sound",            HTTP_OPTIONS, handleOptions);
  server.on("/api/color-sensor",     HTTP_OPTIONS, handleOptions);
  server.on("/api/state",            HTTP_OPTIONS, handleOptions);

  // GET handlers (wrap with CORS header injection)
  server.on("/api/light",  HTTP_GET, []() { addCORSHeaders(); handleLight();        });
  server.on("/api/mode",   HTTP_GET, []() { addCORSHeaders(); handleMode();         });
  server.on("/api/color",  HTTP_GET, []() { addCORSHeaders(); handleColor();        });
  server.on("/api/fountain",HTTP_GET,[]() { addCORSHeaders(); handleFountain();     });
  server.on("/api/gate",   HTTP_GET, []() { addCORSHeaders(); handleGate();         });
  server.on("/api/gate/status", HTTP_GET, []() { addCORSHeaders(); handleGateStatus(); });
  server.on("/api/audio/play",  HTTP_GET, []() { addCORSHeaders(); handleAudioPlay();  });
  server.on("/api/audio/pause", HTTP_GET, []() { addCORSHeaders(); handleAudioPause(); });
  server.on("/api/audio/stop",  HTTP_GET, []() { addCORSHeaders(); handleAudioStop();  });
  server.on("/api/audio/volume",HTTP_GET, []() { addCORSHeaders(); handleAudioVolume();});
  server.on("/api/audio/files", HTTP_GET, []() { addCORSHeaders(); handleAudioFiles(); });
  server.on("/api/sound-reactive",HTTP_GET,[]() { addCORSHeaders(); handleSoundReactive(); });
  server.on("/api/sound",  HTTP_GET, []() { addCORSHeaders(); handleSound();        });
  server.on("/api/color-sensor",HTTP_GET,[]() { addCORSHeaders(); handleColorSensor(); });
  server.on("/api/state",      HTTP_GET,[]() { addCORSHeaders(); handleState();       });

  server.onNotFound([]() {
    addCORSHeaders();
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// TOUCH CALIBRATION
// ═══════════════════════════════════════════════════════════════════════════════

void setupTouch() {
  uint16_t calData[5];
  uint8_t  calOK = 0;

  if (!SPIFFS.begin()) { SPIFFS.format(); SPIFFS.begin(); }

  if (SPIFFS.exists(CALIBRATION_FILE) && !REPEAT_CAL) {
    File f = SPIFFS.open(CALIBRATION_FILE, "r");
    if (f) { if (f.readBytes((char*)calData, 14) == 14) calOK = 1; f.close(); }
  }

  if (calOK) {
    tft.setTouch(calData);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.println("Touch corners as indicated");
    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) { f.write((const unsigned char*)calData, 14); f.close(); }
    tft.setTouch(calData);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// AUDIO TASK  (runs on core 0 so TFT redraws on core 1 never stutter)
// ═══════════════════════════════════════════════════════════════════════════════

void audioTask(void*) { for (;;) { audio.loop(); vTaskDelay(1); } }

// ═══════════════════════════════════════════════════════════════════════════════
// SETUP / LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  // ── Display ─────────────────────────────────────────────────────────────
  tft.init();
  tft.setRotation(1);
  setupTouch();
  tft.fillScreen(C_BG);

  // ── SD card ──────────────────────────────────────────────────────────────
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOk = SD.begin(SD_CS, sdSPI, 16000000);
  if (sdOk) { scanTracks(); Serial.printf("[SD] %d track(s) found\n", trackCount); }
  else        Serial.println("[SD] Card not found");

  // ── I2S audio ────────────────────────────────────────────────────────────
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volToAudio(volume));
  xTaskCreatePinnedToCore(audioTask, "audio", 10000, NULL, 2, NULL, 0);

  // ── Mic ──────────────────────────────────────────────────────────────────
  pinMode(MIC_PIN, INPUT);

  // ── Gate pin ─────────────────────────────────────────────────────────────
  pinMode(GATE_PIN, OUTPUT);
  digitalWrite(GATE_PIN, LOW);  // gate closed on boot

  // ── WiFi ─────────────────────────────────────────────────────────────────
  // Show connecting screen briefly
  tft.fillScreen(C_BG);
  txt("Connecting to WiFi...", W / 2, H / 2, 2, C_DIM, C_BG, MC_DATUM);
  txt(WIFI_SSID, W / 2, H / 2 + 24, 2, C_GREEN, C_BG, MC_DATUM);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    char ipMsg[64];
    snprintf(ipMsg, sizeof(ipMsg), "IP: %s", WiFi.localIP().toString().c_str());
    tft.fillRect(0, H / 2 + 48, W, 24, C_BG);
    txt(ipMsg, W / 2, H / 2 + 52, 2, C_GREEN, C_BG, MC_DATUM);
    delay(1200);
  } else {
    Serial.println("[WiFi] Failed — offline mode");
    tft.fillRect(0, H / 2 + 48, W, 24, C_BG);
    txt("WiFi failed — offline mode", W / 2, H / 2 + 52, 2, C_RED, C_BG, MC_DATUM);
    delay(1200);
  }

  // ── HTTP routes ──────────────────────────────────────────────────────────
  setupRoutes();
  server.begin();
  Serial.println("[HTTP] Server started");

  // ── Gate start ───────────────────────────────────────────────────────────
  enterGateState(GS_WAITING);
}

void loop() {
  // HTTP requests — handled on same core as loop (core 1)
  server.handleClient();

  // Gate state machine (timed transitions)
  pollGate();

  // Touch polling every 25 ms
  static uint32_t touchMs = 0;
  if (millis() - touchMs >= 25) { touchMs = millis(); pollTouch(); }

  // Sensor / animation updates (lights only when gate is open)
  if (gateState == GS_OPEN) {
    updateLive();
    autoAdvance();
  }
}
