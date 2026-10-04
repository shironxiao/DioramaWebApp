/*
  Silvestre del Moro Park - TFT UI v3 (WHITE theme, Lights + Audio)
  ESP32 + ILI9488 480x320 + XPT2046 touch + SD audio (I2S)

  Requires in User_Setup.h:  #define TOUCH_CS 21
  Libraries: TFT_eSPI, ESP32-audioI2S (schreibfaul1)

  Wiring:
    TFT/touch  : per your User_Setup.h (T_CLK 18, T_DIN 23, T_DO 19, T_CS 21)
    SD card    : SCK 14, MISO 33, MOSI 25, CS 13   (separate bus from TFT)
    I2S amp    : BCLK 26, LRC 27, DIN 22           (e.g. MAX98357A)
    Sound mic  : GPIO 34 (analog)

  Diorama hardware pins:
    Light    = GPIO 26  (I2S_BCLK shares — disconnect when using audio)
    Fountain = GPIO 33  (SD_MISO shares  — disconnect when using audio)
    Red      = GPIO 13  (SD_CS)
    Green    = GPIO 14  (SD_SCK)
    Blue     = GPIO 25  (SD_MOSI)
    NOTE: The RGB + Fountain pins overlap with the SD/I2S bus.
    Populate showZones() / setup() based on which features you have wired.
*/

// =====================================================
// TYPES — must be included before Arduino auto-protos
// =====================================================
#include "Diorama_types.h"

// =====================================================
// LIBRARIES
// =====================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <SPIFFS.h>
#include "Audio.h"

// =====================================================
// PINS — SD / I2S / MIC
// =====================================================
#define SD_SCK    14
#define SD_MISO   33
#define SD_MOSI   25
#define SD_CS     13
#define I2S_BCLK  26
#define I2S_LRC   27
#define I2S_DOUT  22
#define MIC_PIN   34   // ADC1 analog sound sensor

// Touch calibration stored in SPIFFS.
// Change CALIBRATION_FILE to force a fresh calibration.
// Set REPEAT_CAL true to always recalibrate on boot.
#define CALIBRATION_FILE "/DioramaCalData"
#define REPEAT_CAL       false

// =====================================================
// DIORAMA RGB HARDWARE PINS
// (overlap with SD/I2S — see note above)
// =====================================================
const int LIGHT_PIN    = 26;
const int FOUNTAIN_PIN = 33;
const int RED_PIN      = 13;
const int GREEN_PIN    = 14;
const int BLUE_PIN     = 25;

// =====================================================
// OBJECTS
// =====================================================
TFT_eSPI  tft = TFT_eSPI();
SPIClass  sdSPI(HSPI);
Audio     audio;

// =====================================================
// COLORS  (white / light-green theme, RGB565)
// =====================================================
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
const uint16_t C_BG     = rgb565(245, 250, 247);
const uint16_t C_CARD   = rgb565(255, 255, 255);
const uint16_t C_HDR    = rgb565(255, 255, 255);
const uint16_t C_BORDER = rgb565(196, 217, 205);
const uint16_t C_TRACK  = rgb565(215, 228, 222);
const uint16_t C_TEXT   = rgb565(20,  35,  30);
const uint16_t C_DIM    = rgb565(120, 140, 132);
const uint16_t C_ONACC  = rgb565(255, 255, 255);
const uint16_t C_GREEN  = rgb565(16,  150, 95);
const uint16_t C_AMBER  = rgb565(230, 130, 0);
const uint16_t C_RED    = rgb565(220, 50,  60);

// =====================================================
// GEOMETRY
// =====================================================
const int W = 480, H = 320, HDR_H = 36;
const int SL_X0 = 25, SL_X1 = 455;
const int HUE_Y = 152, SAT_Y = 180;

// =====================================================
// COLOR HELPERS
// =====================================================

RGB hsv(int h, int s, int v) {   // h 0-359, s/v 0-100
  float S = s / 100.f, V = v / 100.f;
  float C = V * S, X = C * (1 - fabsf(fmodf(h / 60.f, 2.f) - 1)), m = V - C;
  float r, g, b;
  switch ((h / 60) % 6) {
    case 0: r = C; g = X; b = 0; break;
    case 1: r = X; g = C; b = 0; break;
    case 2: r = 0; g = C; b = X; break;
    case 3: r = 0; g = X; b = C; break;
    case 4: r = X; g = 0; b = C; break;
    default: r = C; g = 0; b = X; break;
  }
  return { (uint8_t)((r + m) * 255 + .5f),
           (uint8_t)((g + m) * 255 + .5f),
           (uint8_t)((b + m) * 255 + .5f) };
}

void rgbToHs(RGB c, int &h, int &s) {
  float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
  float mx = max(r, max(g, b)), mn = min(r, min(g, b)), d = mx - mn, hh = 0;
  if (d > 0) {
    if (mx == r)      hh = 60 * fmodf((g - b) / d, 6.f);
    else if (mx == g) hh = 60 * ((b - r) / d + 2);
    else              hh = 60 * ((r - g) / d + 4);
    if (hh < 0) hh += 360;
  }
  h = (int)hh % 360;
  s = mx == 0 ? 0 : (int)(d / mx * 100);
}

uint16_t c565(RGB c) { return tft.color565(c.r, c.g, c.b); }

const RGB PAL[12] = {
  {255,68,68},{255,140,0},{255,215,0},{124,252,0},{0,191,255},{138,43,226},
  {255,105,180},{255,255,255},{0,206,209},{255,99,71},{65,105,225},{50,205,50}
};

// =====================================================
// STATE
// =====================================================
Page page      = P_LIGHTS;
Mode mode      = M_BASIC;
Drag dragging  = D_NONE;

bool lightsOn    = true;
bool fountainOn  = false;
int  brightness  = 100;

int  hue = 36, sat = 46;
RGB  cur  = hsv(36, 46, 100);
RGB  zone[3] = { {255,180,90}, {255,180,90}, {255,180,90} };
int  lastApplied = -1;   // 0=ALL, 1=Left, 2=Right, 3=Center

int   sens    = 60;
float lvl     = 0;
int   hueBase = 0;
RGB   live    = {255, 180, 90};

// Audio
#define MAX_TRACKS 24
char     trackPath[MAX_TRACKS][48];
int      trackCount = 0, curTrack = 0, volume = 60;
bool     sdOk = false, audioPlaying = false, audioStarted = false;
uint32_t trackStartMs = 0;

bool wasTouched = false;

void onDrag(int x);   // forward declaration

// =====================================================
// HARDWARE HOOKS
// =====================================================

// Apply final (brightness-scaled) colors to physical RGB output.
// Center zone drives the single RGB LED strip currently wired.
void showZones(RGB left, RGB right, RGB center) {
  analogWrite(RED_PIN,   center.r);
  analogWrite(GREEN_PIN, center.g);
  analogWrite(BLUE_PIN,  center.b);
  // Fountain pin is shared with SD_MISO on this wiring — only drive it
  // when SD is not in use, or rewire to a free GPIO.
  // digitalWrite(FOUNTAIN_PIN, fountainOn ? HIGH : LOW);
}

// Return true and fill *out if a color sensor (e.g. TCS34725) is connected.
bool readColorSensor(RGB *out) {
  return false;   // no sensor wired yet
}

// =====================================================
// ZONE CONTROL
// =====================================================

void pushZones() {
  RGB o[3];
  for (int i = 0; i < 3; i++) {
    if (lightsOn)
      o[i] = { (uint8_t)(zone[i].r * brightness / 100),
               (uint8_t)(zone[i].g * brightness / 100),
               (uint8_t)(zone[i].b * brightness / 100) };
    else
      o[i] = {0, 0, 0};
  }
  showZones(o[0], o[1], o[2]);
}

void setAll(RGB c) { zone[0] = zone[1] = zone[2] = c; pushZones(); }

// =====================================================
// DRAWING HELPERS
// =====================================================

bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void text(const char* s, int x, int y, int font, uint16_t fg, uint16_t bg,
          uint8_t datum = TL_DATUM) {
  tft.setTextColor(fg, bg);
  tft.setTextDatum(datum);
  tft.drawString(s, x, y, font);
}

void card(int x, int y, int w, int h, uint16_t fill, uint16_t border) {
  tft.fillRoundRect(x, y, w, h, 8, fill);
  tft.drawRoundRect(x, y, w, h, 8, border);
}

void button(int x, int y, int w, int h, const char* label, bool active, uint16_t accent) {
  card(x, y, w, h, active ? accent : C_CARD, active ? accent : C_BORDER);
  text(label, x + w / 2, y + h / 2, 2,
       active ? C_ONACC : C_TEXT,
       active ? accent  : C_CARD,
       MC_DATUM);
}

void toggleSwitch(int x, int y, bool on, uint16_t accent) {
  tft.fillRoundRect(x, y, 44, 22, 11, on ? accent : C_TRACK);
  tft.fillCircle(on ? x + 33 : x + 11, y + 11, 8, C_CARD);
  tft.drawCircle(on ? x + 33 : x + 11, y + 11, 8, C_BORDER);
}

void drawSlider(int y, int val, uint16_t accent) {
  tft.fillRect(10, y - 16, 460, 32, C_BG);
  tft.fillRoundRect(SL_X0, y - 4, SL_X1 - SL_X0, 8, 4, C_TRACK);
  int kx = SL_X0 + (long)(SL_X1 - SL_X0) * val / 100;
  if (kx > SL_X0) tft.fillRoundRect(SL_X0, y - 4, kx - SL_X0, 8, 4, accent);
  tft.fillCircle(kx, y, 11, accent);
  tft.drawCircle(kx, y, 11, C_TEXT);
}

void drawGradSlider(int y, bool isHue) {
  tft.fillRect(10, y - 14, 460, 28, C_BG);
  for (int x = SL_X0; x < SL_X1; x += 5) {
    int p = (long)(x - SL_X0) * 100 / (SL_X1 - SL_X0);
    RGB c = isHue ? hsv(p * 359 / 100, 100, 100) : hsv(hue, p, 100);
    tft.fillRect(x, y - 4, 5, 8, c565(c));
  }
  int kx = SL_X0 + (long)(SL_X1 - SL_X0) *
            (isHue ? hue * 100 / 359 : sat) / 100;
  tft.fillCircle(kx, y, 11, c565(isHue ? hsv(hue, 100, 100) : cur));
  tft.drawCircle(kx, y, 11, C_TEXT);
}

int pctFromX(int x) {
  return constrain((long)(x - SL_X0) * 100 / (SL_X1 - SL_X0), 0, 100);
}

void drawPct(int xRight, int y, int val, uint16_t col) {
  char b[8];
  snprintf(b, sizeof(b), "%d%%", val);
  tft.setTextPadding(60);
  text(b, xRight, y, 4, col, C_BG, MR_DATUM);
  tft.setTextPadding(0);
}

// =====================================================
// HEADER
// =====================================================

void drawHeader() {
  tft.fillRect(0, 0, W, HDR_H, C_HDR);
  tft.drawFastHLine(0, HDR_H - 1, W, C_BORDER);
  text("Silvestre del Moro Park", 8, HDR_H / 2, 2, C_TEXT, C_HDR, ML_DATUM);

  bool a = (page == P_LIGHTS), b = (page == P_AUDIO);
  tft.fillRect(250, 0, 105, HDR_H - 1, a ? C_AMBER : C_HDR);
  text("Lights", 302, HDR_H / 2, 2, a ? C_ONACC : C_DIM, a ? C_AMBER : C_HDR, MC_DATUM);
  tft.fillRect(355, 0, 105, HDR_H - 1, b ? C_GREEN : C_HDR);
  text("Audio",  407, HDR_H / 2, 2, b ? C_ONACC : C_DIM, b ? C_GREEN : C_HDR, MC_DATUM);

  // SD card status dot
  tft.fillCircle(472, HDR_H / 2, 5, sdOk ? C_GREEN : C_RED);
}

// =====================================================
// LIGHTS — BASIC
// =====================================================

void drawBasic() {
  card(10, 90, 460, 56, C_CARD, lightsOn ? C_AMBER : C_BORDER);
  text("Lights", 24, 106, 4, C_TEXT, C_CARD, ML_DATUM);
  text(lightsOn ? "On - steady warm" : "Off",
       24, 132, 2, lightsOn ? C_AMBER : C_DIM, C_CARD, ML_DATUM);
  toggleSwitch(414, 107, lightsOn, C_AMBER);
  text("Brightness", 10, 170, 2, C_DIM, C_BG, ML_DATUM);
  drawPct(W - 12, 178, brightness, C_AMBER);
  drawSlider(215, brightness, C_AMBER);
}

// =====================================================
// LIGHTS — COLOR
// =====================================================

void drawColorHeader() {
  tft.fillRect(10, 84, 460, 56, C_BG);
  tft.fillRoundRect(10, 86, 52, 52, 8, c565(cur));
  tft.drawRoundRect(10, 86, 52, 52, 8, C_TEXT);
  char b[24];
  snprintf(b, sizeof(b), "#%02X%02X%02X", cur.r, cur.g, cur.b);
  text(b, 72, 88, 4, C_TEXT, C_BG);
  snprintf(b, sizeof(b), "R %d  G %d  B %d", cur.r, cur.g, cur.b);
  text(b, 72, 120, 2, C_DIM, C_BG);
  button(340, 92, 130, 40, "Scan sensor", false, C_AMBER);
}

void drawApply() {
  const char* n[4] = { "ALL", "Left", "Right", "Center" };
  text("Apply color to", 10, 232, 2, C_DIM, C_BG, ML_DATUM);
  for (int i = 0; i < 4; i++) {
    int x = 10 + i * 118;
    bool act = (lastApplied == i);
    uint16_t f = act ? C_AMBER : C_CARD;
    card(x, 254, 112, 52, f, act ? C_AMBER : C_BORDER);
    RGB dc = (i == 0) ? cur : zone[i - 1];
    tft.fillCircle(x + 16, 280, 8, c565(dc));
    tft.drawCircle(x + 16, 280, 8, C_TEXT);
    text(n[i], x + 66, 280, 2, act ? C_ONACC : C_TEXT, f, MC_DATUM);
  }
}

void drawColorPanel() {
  drawColorHeader();
  drawGradSlider(HUE_Y, true);
  drawGradSlider(SAT_Y, false);
  for (int i = 0; i < 12; i++) {
    int cx = 29 + i * 38;
    tft.fillCircle(cx, 212, 13, c565(PAL[i]));
    tft.drawCircle(cx, 212, 13, C_BORDER);
  }
  drawApply();
}

// =====================================================
// LIGHTS — SOUND
// =====================================================

void drawLevel() {
  int fw = (int)(lvl * 460);
  if (fw > 0)    tft.fillRect(10,      170, fw,       26, c565(live));
  if (fw < 460)  tft.fillRect(10 + fw, 170, 460 - fw, 26, C_TRACK);
}

void drawSound() {
  text("All RGB lights follow the sound", 10, 92, 2, C_TEXT, C_BG);
  text("Play a track in Audio and watch them react.", 10, 114, 2, C_DIM, C_BG);
  text("Level", 10, 148, 2, C_DIM, C_BG);
  drawLevel();
  text("Sensitivity", 10, 226, 2, C_DIM, C_BG, ML_DATUM);
  drawPct(W - 12, 230, sens, C_AMBER);
  drawSlider(262, sens, C_AMBER);
}

// =====================================================
// LIGHTS — ADAPTIVE
// =====================================================

void drawAdaptSwatch() {
  tft.fillRoundRect(10, 130, 460, 90, 8, c565(live));
  tft.drawRoundRect(10, 130, 460, 90, 8, C_BORDER);
}

void drawAdapt() {
  text("Follows the ambient color sensor", 10, 92, 2, C_TEXT, C_BG);
  text("Lights update automatically.",     10, 112, 2, C_DIM,  C_BG);
  drawAdaptSwatch();
}

// =====================================================
// LIGHTS PAGE
// =====================================================

void drawLights() {
  tft.fillRect(0, HDR_H, W, H - HDR_H, C_BG);
  const char* mn[4] = { "Basic", "Colorful", "Sound", "Adaptive" };
  for (int i = 0; i < 4; i++)
    button(10 + i * 118, 42, 112, 36, mn[i], mode == i, C_AMBER);
  switch (mode) {
    case M_BASIC: drawBasic();      break;
    case M_COLOR: drawColorPanel(); break;
    case M_SOUND: drawSound();      break;
    case M_ADAPT: drawAdapt();      break;
  }
}

// =====================================================
// AUDIO PAGE
// =====================================================

int volToAudio() { return map(volume, 0, 100, 0, 21); }

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
      if (n[0] != '.' &&
          (l.endsWith(".mp3") || l.endsWith(".wav") ||
           l.endsWith(".aac") || l.endsWith(".m4a") || l.endsWith(".flac"))) {
        snprintf(trackPath[trackCount++], sizeof(trackPath[0]), "/%s", n.c_str());
      }
    }
    f.close();
    f = root.openNextFile();
  }
  root.close();
  // Sort alphabetically
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
  Serial.printf("[AUDIO] Playing Track %d/%d: %s\n", curTrack + 1, trackCount, trackPath[i]);
}

void togglePlay() {
  if (!trackCount) return;
  if (!audioStarted) startTrack(curTrack);
  else {
    audio.pauseResume();
    audioPlaying = !audioPlaying;
    Serial.printf("[AUDIO] Playback state: %s (Track: %s)\n", audioPlaying ? "PLAYING" : "PAUSED", trackPath[curTrack]);
  }
}

void stepTrack(int d) {
  if (!trackCount) return;
  int n = (curTrack + d + trackCount) % trackCount;
  Serial.printf("[AUDIO] Track changed: %s -> %s\n", trackPath[curTrack], trackPath[n]);
  if (audioStarted) startTrack(n); else curTrack = n;
}

void trackTitle(char* out, size_t n) {
  strlcpy(out, trackPath[curTrack] + 1, n);
  char* d = strrchr(out, '.');
  if (d) *d = 0;
  if (strlen(out) > 26) { out[24] = '.'; out[25] = '.'; out[26] = 0; }
}

void drawAudio() {
  tft.fillRect(0, HDR_H, W, H - HDR_H, C_BG);
  char t[48], c[24];
  if (!sdOk)
    text("SD card not found",    W / 2, 62, 4, C_RED,   C_BG, MC_DATUM);
  else if (!trackCount)
    text("No audio files on SD", W / 2, 62, 4, C_AMBER, C_BG, MC_DATUM);
  else {
    trackTitle(t, sizeof(t));
    text(t, W / 2, 62, 4, C_TEXT, C_BG, MC_DATUM);
    snprintf(c, sizeof(c), "Track %d / %d", curTrack + 1, trackCount);
    text(c, W / 2, 90, 2, C_DIM, C_BG, MC_DATUM);
  }
  text(audioPlaying ? "Playing" : (audioStarted ? "Paused" : "Stopped"),
       W / 2, 112, 2,
       audioPlaying ? C_GREEN : C_DIM, C_BG, MC_DATUM);
  button( 70, 134, 100, 64, "<<",                  false,        C_GREEN);
  button(190, 134, 100, 64, audioPlaying ? "Pause" : "Play", audioPlaying, C_GREEN);
  button(310, 134, 100, 64, ">>",                  false,        C_GREEN);
  text("Volume", 10, 222, 2, C_DIM, C_BG, ML_DATUM);
  drawPct(W - 12, 226, volume, C_GREEN);
  drawSlider(262, volume, C_GREEN);
}

void autoAdvance() {
  if (audioPlaying && audioStarted &&
      millis() - trackStartMs > 1000 && !audio.isRunning()) {
    stepTrack(1);
    if (page == P_AUDIO) drawAudio();
  }
}

// =====================================================
// SOUND SENSOR
// =====================================================

float readLevel() {
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
  static uint32_t last = 0, lastA = 0;
  if (millis() - last < 40) return;
  last = millis();

  if (mode == M_SOUND && lightsOn) {
    lvl     = max(readLevel(), lvl * 0.82f);
    hueBase = (hueBase + 2) % 360;
    live    = hsv((hueBase + (int)(lvl * 120)) % 360, 100, 12 + (int)(88 * lvl));
    setAll(live);
    if (page == P_LIGHTS) drawLevel();

  } else if (mode == M_ADAPT && millis() - lastA > 500) {
    lastA = millis();
    RGB c;
    if (readColorSensor(&c)) {
      live = c;
      setAll(c);
      if (page == P_LIGHTS) drawAdaptSwatch();
    }
  }
}

// =====================================================
// MODE / APPLY
// =====================================================

void setMode(Mode m) {
  mode     = m;
  lightsOn = true;
  const char* mn[4] = { "Basic", "Colorful", "Sound", "Adaptive" };
  Serial.printf("[UI] Light Mode changed to: %s\n", mn[m]);
  if (m == M_BASIC) setAll({255, 180, 90});
  else pushZones();
}

void applyColor(int i) {
  if (i == 0) zone[0] = zone[1] = zone[2] = cur;
  else zone[i - 1] = cur;
  lastApplied = i;
  lightsOn    = true;
  const char* n[4] = { "ALL", "Left", "Right", "Center" };
  Serial.printf("[COLOR] Color #%02X%02X%02X applied to Zone: %s\n", cur.r, cur.g, cur.b, n[i]);
  pushZones();
  drawApply();
}

// =====================================================
// TOUCH
// =====================================================

void onPress(int x, int y) {
  // Header: page tabs
  if (y < HDR_H) {
    Page np = (x >= 355) ? P_AUDIO : (x >= 250 ? P_LIGHTS : page);
    if (np != page) {
      page = np;
      Serial.printf("[UI] Page switched: %s\n", page == P_LIGHTS ? "LIGHTS" : "AUDIO");
      drawHeader();
      if (page == P_LIGHTS) drawLights(); else drawAudio();
    }
    return;
  }

  // Audio page
  if (page == P_AUDIO) {
    if      (inRect(x, y, 190, 134, 100, 64)) { togglePlay(); drawAudio(); }
    else if (inRect(x, y,  70, 134, 100, 64)) { stepTrack(-1); drawAudio(); }
    else if (inRect(x, y, 310, 134, 100, 64)) { stepTrack(1);  drawAudio(); }
    else if (y > 240 && y < 288)              { dragging = D_VOL; }
    if (dragging) onDrag(x);
    return;
  }

  // Lights page — mode tabs
  for (int i = 0; i < 4; i++)
    if (inRect(x, y, 10 + i * 118, 42, 112, 36)) {
      setMode((Mode)i); drawLights(); return;
    }

  if (mode == M_BASIC) {
    if (inRect(x, y, 10, 90, 460, 56)) {
      lightsOn = !lightsOn;
      Serial.printf("[LIGHTS] Power toggled: %s\n", lightsOn ? "ON" : "OFF");
      pushZones();
      drawLights();
    }
    else if (y > 193 && y < 238) { dragging = D_BR; }
  }
  else if (mode == M_COLOR) {
    if (inRect(x, y, 340, 92, 130, 40)) {
      Serial.println("[COLOR] Scan sensor button pressed");
      RGB c;
      if (readColorSensor(&c)) {
        cur = c; rgbToHs(c, hue, sat);
        Serial.printf("[COLOR] Sensor read RGB: #%02X%02X%02X\n", cur.r, cur.g, cur.b);
        drawColorPanel();
      } else {
        Serial.println("[COLOR] Sensor read failed: No sensor found");
        text("No sensor found", 72, 120, 2, C_RED, C_BG);
      }
    }
    else if (y > 137 && y < 164) { dragging = D_HUE; }
    else if (y > 166 && y < 194) { dragging = D_SAT; }
    else if (y > 196 && y < 228) {
      for (int i = 0; i < 12; i++)
        if (abs(x - (29 + i * 38)) <= 19) {
          cur = PAL[i]; rgbToHs(cur, hue, sat);
          Serial.printf("[COLOR] Preset color selected: #%02X%02X%02X (Hue: %d, Sat: %d%%)\n", cur.r, cur.g, cur.b, hue, sat);
          drawColorHeader();
          drawGradSlider(HUE_Y, true);
          drawGradSlider(SAT_Y, false);
          drawApply();
          break;
        }
    }
    else if (y >= 254 && y < 306) {
      for (int i = 0; i < 4; i++)
        if (inRect(x, y, 10 + i * 118, 254, 112, 52)) { applyColor(i); break; }
    }
  }
  else if (mode == M_SOUND) {
    if (y > 240 && y < 288) dragging = D_SENS;
  }

  if (dragging) onDrag(x);
}

void onDrag(int x) {
  int p = pctFromX(x);
  switch (dragging) {
    case D_BR:
      if (p != brightness) {
        brightness = p;
        Serial.printf("[LIGHTS] Brightness changed: %d%%\n", brightness);
        drawPct(W - 12, 178, p, C_AMBER);
        drawSlider(215, p, C_AMBER);
        pushZones();
      }
      break;
    case D_HUE: {
      int h = p * 359 / 100;
      if (h != hue) {
        hue = h; cur = hsv(hue, sat, 100);
        Serial.printf("[COLOR] Hue changed: %d° -> RGB #%02X%02X%02X\n", hue, cur.r, cur.g, cur.b);
        drawGradSlider(HUE_Y, true);
        drawGradSlider(SAT_Y, false);
        drawColorHeader();
      }
      break;
    }
    case D_SAT:
      if (p != sat) {
        sat = p; cur = hsv(hue, sat, 100);
        Serial.printf("[COLOR] Saturation changed: %d%% -> RGB #%02X%02X%02X\n", sat, cur.r, cur.g, cur.b);
        drawGradSlider(SAT_Y, false);
        drawColorHeader();
      }
      break;
    case D_SENS:
      if (p != sens) {
        sens = p;
        Serial.printf("[SOUND] Sensitivity changed: %d%%\n", sens);
        drawPct(W - 12, 230, p, C_AMBER);
        drawSlider(262, p, C_AMBER);
      }
      break;
    case D_VOL:
      if (p != volume) {
        volume = p;
        Serial.printf("[AUDIO] Volume changed: %d%%\n", volume);
        drawPct(W - 12, 226, p, C_GREEN);
        drawSlider(262, p, C_GREEN);
        audio.setVolume(volToAudio());
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
    if (dragging == D_HUE || dragging == D_SAT) drawApply();
    dragging = D_NONE;
  }
}

// =====================================================
// TOUCH CALIBRATION  (SPIFFS-backed; set REPEAT_CAL true to redo)
// =====================================================

void setupTouch() {
  uint16_t calData[5];
  uint8_t  calDataOK = 0;

  // Mount SPIFFS, format if needed
  if (!SPIFFS.begin()) {
    Serial.println("Formatting SPIFFS...");
    SPIFFS.format();
    SPIFFS.begin();
  }

  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      SPIFFS.remove(CALIBRATION_FILE);   // force redo
    } else {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14) calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    tft.setTouch(calData);
  } else {
    // Run on-screen calibration wizard
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("Touch corners as indicated");
    tft.setTextFont(1);
    tft.println();
    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }
    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // Persist to SPIFFS
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
    tft.setTouch(calData);
  }
}


// =====================================================
// AUDIO TASK  (core 0, so redraws on core 1 never stutter)
// =====================================================

void audioTask(void*) {
  for (;;) { audio.loop(); vTaskDelay(1); }
}

// =====================================================
// SETUP / LOOP
// =====================================================

void setup() {
  Serial.begin(115200);

  // Display
  tft.init();
  tft.setRotation(1);
  setupTouch();
  tft.fillScreen(C_BG);

  // SD card (HSPI bus, separate from TFT)
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOk = SD.begin(SD_CS, sdSPI, 16000000);
  if (sdOk) scanTracks();

  // I2S audio amp
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volToAudio());
  xTaskCreatePinnedToCore(audioTask, "audio", 10000, NULL, 2, NULL, 0);

  // Analog mic
  pinMode(MIC_PIN, INPUT);

  // RGB output pins
  // NOTE: These overlap with SD/I2S — populate only when audio is not wired.
  // pinMode(RED_PIN,   OUTPUT);
  // pinMode(GREEN_PIN, OUTPUT);
  // pinMode(BLUE_PIN,  OUTPUT);
  // pinMode(FOUNTAIN_PIN, OUTPUT);

  // Initial state
  setAll({255, 180, 90});
  drawHeader();
  drawLights();
}

void loop() {
  static uint32_t lt = 0;
  if (millis() - lt >= 25) { lt = millis(); pollTouch(); }
  updateLive();
  autoAdvance();
}
