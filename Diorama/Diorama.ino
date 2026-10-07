/*
  Silvestre del Moro Park — Diorama Controller v4
  ESP32 + ILI9488 480×320 + XPT2046 touch + rotary encoder + WiFi HTTP server

  TFT scope  : Gate access screen  |  Lights (Basic / Sound / Adaptive)  |  Audio (temporarily disabled)
  Web App    : Full control via HTTP GET endpoints (same ESP32 state)

  ── Wiring ──────────────────────────────────────────────────────────────────
  TFT SPI      : SCK 18, MOSI 23, TFT_CS 15, DC 2, RST 4
  Touch        : XPT2046 shares SPI; T_CLK 18, T_DIN 23, T_DO 19, T_CS 21
  Note         : Leave ILI9488 SDO disconnected; only XPT2046 T_DO connects to GPIO 19.
  Rotary      : CLK 25, DT 26, SW 27
  SD/I2S      : Temporarily disabled; GPIO 25-27 are reserved for the encoder.
  Mic         : GPIO 34  (ADC1 analog)
  RGB LEDs    : red disabled (GPIO 4 is TFT reset), G=5, B=12
  Shared I2C bus: ESP32 SDA=32, SCL=33 -> VEML7700 and PCA9685 SDA/SCL
  Color controller UART1: Nano TX -> GPIO 36, ESP32 TX GPIO 22 -> Nano RX, 9600 baud
  Color UART packet, one line at 9600 baud: JSON RGB or three RGB numbers
  Fountain    : GPIO 2            (boot-strapping pin; hardware output not enabled)
  Fingerprint : RX2=16 ← sensor TX,  TX2=17 → sensor RX  (UART2)
  Gate actuator: not configured (GPIO 15 is the TFT chip-select pin)

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
  Note: Audio control endpoints return 503 while SD_AUDIO_ENABLED is false.
  GET /api/sound-reactive?state=on|off&intensity=0-100
  GET /api/sound                                → {"detected":bool,"level":0-1023}
*/

// ── Types — defined inline to avoid header include issues ───────────────────
struct RGB    { uint8_t r, g, b; };   // must be first — used in function signatures
enum Page     { P_LIGHTS = 0, P_AUDIO = 1, P_SETTINGS = 2 };
enum Mode     { M_BASIC = 0, M_COLOR = 1, M_SOUND = 2, M_ADAPT = 3 };
enum Drag     { D_NONE = 0, D_BR, D_HUE, D_SAT, D_SENS, D_VOL };
enum GateState { GS_WAITING = 0, GS_SCANNING = 1, GS_OPEN = 2, GS_GOODBYE = 3 };
enum EncoderMode { ENC_NAVIGATE, ENC_FOCUS, ENC_ADJUST };
enum EncoderControl {
  ENC_CONTROL_NONE,
  ENC_CONTROL_LIGHT_MODE,
  ENC_CONTROL_LIGHT_POWER,
  ENC_CONTROL_BRIGHTNESS,
  ENC_CONTROL_HUE,
  ENC_CONTROL_SATURATION,
  ENC_CONTROL_SENSITIVITY,
  ENC_CONTROL_VOLUME,
  ENC_CONTROL_AUDIO_PREVIOUS,
  ENC_CONTROL_AUDIO_PLAY,
  ENC_CONTROL_AUDIO_NEXT,
  ENC_CONTROL_COLOR_ALL,
  ENC_CONTROL_COLOR_LEFT,
  ENC_CONTROL_COLOR_RIGHT,
  ENC_CONTROL_COLOR_CENTER,
  ENC_CONTROL_SETTINGS_ENROLL,
  ENC_CONTROL_SETTINGS_DELETE_0,
  ENC_CONTROL_SETTINGS_DELETE_1,
  ENC_CONTROL_SETTINGS_DELETE_2,
  ENC_CONTROL_SETTINGS_DELETE_3,
  ENC_CONTROL_SETTINGS_DELETE_4,
  ENC_CONTROL_SETTINGS_DELETE_5
};

// User structure for fingerprint management
struct FingerprintUser {
  int id;
  char name[32];
  bool active;
};

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
#include <Wire.h>
#include <Adafruit_Fingerprint.h>
#include "webapp_embed.h"

#if TFT_CS == TOUCH_CS
  #error "TFT_CS and TOUCH_CS must be connected to separate GPIO pins"
#endif

// ── WiFi — Access Point mode ──────────────────────────────────────────────────
// The ESP32 creates its own WiFi network.
// Phone/PC connects to this network, then opens http://192.168.4.1
// to reach the web app (if served from ESP32 SPIFFS) or uses the IP
// shown on the TFT to configure the web app's Settings page.
#define WIFI_AP_SSID  "Diorama-Park"
#define WIFI_AP_PASS  "diorama123"   // min 8 chars; set "" for open network

// ── Pins ─────────────────────────────────────────────────────────────────────
#define SD_SCK    14
#define SD_MISO   35
#define SD_MOSI  25  // Unused while SD_AUDIO_ENABLED is false; reassign before enabling.
#define SD_CS     13
#define I2S_BCLK  26  // Unused while SD_AUDIO_ENABLED is false; reassign before enabling.
#define I2S_LRC   27  // Unused while SD_AUDIO_ENABLED is false; reassign before enabling.
#define I2S_DOUT  22  // Also used by color UART ACK; reassign before enabling audio.
#define MIC_PIN   34
#define ENCODER_CLK_PIN 25
#define ENCODER_DT_PIN  26
#define ENCODER_SW_PIN  27

constexpr bool SD_AUDIO_ENABLED = false;

#define GREEN_PIN    5
#define BLUE_PIN     12
#define I2C_SDA_PIN 32  // Shared by the ambient sensor and PWM extender
#define I2C_SCL_PIN 33
#define COLOR_SENSOR_RX_PIN 36
#define COLOR_SENSOR_TX_PIN 22
#define COLOR_SENSOR_BAUD   9600
#define COLOR_SENSOR_TIMEOUT_MS 5000
#define FOUNTAIN_PIN 2

// Fingerprint sensor pins (UART2)
#define FP_RX_PIN    16
#define FP_TX_PIN    17

// Calibration
#define CALIBRATION_FILE "/DioramaCalData"
#define REPEAT_CAL       false  // Recalibrate after correcting the shared MISO wiring

// User data file
#define USERS_FILE "/fingerprint_users.json"
#define MAX_USERS 50

// ── Objects ───────────────────────────────────────────────────────────────────
TFT_eSPI   tft = TFT_eSPI();
SPIClass   sdSPI(HSPI);
Audio      audio;
WebServer  server(80);

// Fingerprint sensor
HardwareSerial fingerprintSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerprintSerial);
HardwareSerial colorSensorSerial(1);

// Fingerprint users array
FingerprintUser users[MAX_USERS];
int userCount = 0;
bool fpSensorAvailable = false;

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
const uint16_t C_FOCUS  = rgb565(  0, 120, 215);

// ── Geometry ──────────────────────────────────────────────────────────────────
//  480 × 320 landscape.  Header = top 36px.  Usable area = y 36..319 (284px).
const int W = 480, H = 320;
const int HDR_H = 36;         // header bar height
const int BODY_Y = HDR_H;     // body starts here
const int BODY_H = H - HDR_H; // 284 px

// Slider rail X extents
const int SL_X0 = 20, SL_X1 = 460;

// ── Colour helpers ────────────────────────────────────────────────────────────

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
RGB     zone[3]     = { {255,255,255}, {255,255,255}, {255,255,255} };
// Working colour for Colorful mode picker
int     hue = 36, sat = 46;
RGB     cur = {255, 255, 255};
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
EncoderMode encoderMode = ENC_NAVIGATE;
EncoderControl encoderControl = ENC_CONTROL_BRIGHTNESS;
uint8_t encoderControlIndex = 0;
uint8_t encoderPreviousState = 0;
int8_t encoderQuarterSteps = 0;
bool encoderButtonStableHigh = true;
bool encoderButtonCandidateHigh = true;
uint32_t encoderButtonChangeMs = 0;
uint32_t encoderButtonDownMs = 0;
bool encoderLongPressHandled = false;

// Sound reactive live level (mic)
int   sens    = 60;
float lvl     = 0;
int   hueBase = 0;
  RGB   live    = {255, 255, 255};

int micRawMin = 0;
int micRawMax = 0;
int micPeakToPeak = 0;
int micRms = 0;
int micLevel = 0;
bool micDetected = false;
uint32_t micLastSampleMs = 0;
uint32_t micLastLogMs = 0;

bool ambientSensorAvailable = false;
bool colorSensorAvailable = false;
bool colorSensorReceiveSeen = false;
bool ambientSensorConfigured = false;
float ambientLux = 0.0f;
RGB detectedColor = {255, 255, 255};
uint32_t colorSensorLastPacketMs = 0;
uint32_t ambientLastDrawMs = 0;

bool wasTouched = false;
uint16_t touchCalData[5] = {};
bool touchCalibrationReady = false;

// Forward declarations
void drawGateScreen();
void enterGateState(GateState s);
void drawHeader();
void drawLights();
void drawAudio();
void drawSettings();
void onDrag(int x);
void pushZones();
void applyHardwareGate(bool open);
void pollEncoder();
void applyEncoderStep(int direction);
void syncEncoderControlIndex();
void applyColorZone(uint8_t zoneIndex);
void selectEncoderControl(EncoderControl control);
void startEnrollment();
bool deleteUser(int fpID);
void togglePlay();
void stepTrack(int direction);
int volToAudio(int v);

// ═══════════════════════════════════════════════════════════════════════════════
// HARDWARE OUTPUT
// ═══════════════════════════════════════════════════════════════════════════════

// Push logical RGB zone colors to the inverted MOSFET PWM outputs.
// All three zones share one RGB strip on current wiring → use zone[2] (center).
void pushZones() {
  for (int i = 0; i < 3; i++) {
    zone[i].r = constrain(zone[i].r, 0, 255);
    zone[i].g = constrain(zone[i].g, 0, 255);
    zone[i].b = constrain(zone[i].b, 0, 255);
  }
  if (lightsOn) {
    analogWrite(GREEN_PIN, 255 - (zone[2].g * brightness / 100));
    analogWrite(BLUE_PIN,  255 - (zone[2].b * brightness / 100));
  } else {
    analogWrite(GREEN_PIN, 255);
    analogWrite(BLUE_PIN,  255);
  }
}

void setAll(RGB c) {
  zone[0] = zone[1] = zone[2] = c;
  pushZones();
}

void applyHardwareGate(bool open) {
  // Replace with servo / relay:
  // myServo.write(open ? 90 : 0);
  Serial.printf("[GATE] Actuator: %s\n", open ? "OPEN" : "CLOSED");
}

bool parseColorSensorPacket(const char* packet, RGB* color) {
  StaticJsonDocument<192> doc;
  if (!deserializeJson(doc, packet) &&
      doc["r"].is<int>() && doc["g"].is<int>() && doc["b"].is<int>()) {
    const int red = doc["r"].as<int>();
    const int green = doc["g"].as<int>();
    const int blue = doc["b"].as<int>();
    if (red < 0 || red > 255 || green < 0 || green > 255 ||
        blue < 0 || blue > 255) return false;
    *color = {(uint8_t)red, (uint8_t)green, (uint8_t)blue};
    return true;
  }

  int values[3];
  uint8_t valueCount = 0;
  const char* cursor = packet;
  while (*cursor) {
    const bool negative = *cursor == '-' && cursor[1] >= '0' && cursor[1] <= '9';
    if ((*cursor >= '0' && *cursor <= '9') || negative) {
      if (valueCount >= 3) return false;
      char* end = nullptr;
      const long value = strtol(cursor, &end, 10);
      if (end == cursor || value < 0 || value > 255) return false;
      values[valueCount++] = (int)value;
      cursor = end;
    } else {
      cursor++;
    }
  }
  if (valueCount != 3) return false;
  *color = {(uint8_t)values[0], (uint8_t)values[1], (uint8_t)values[2]};
  return true;
}

void processColorSensorPacket(const char* packet) {
  RGB received;
  if (!parseColorSensorPacket(packet, &received)) {
    static uint32_t invalidPacketLogMs = 0;
    const uint32_t now = millis();
    if (now - invalidPacketLogMs >= 1000) {
      invalidPacketLogMs = now;
      Serial.printf("[COLOR UART] Unrecognized line: '%s' (expected JSON RGB or three values, e.g. 120,80,200)\n",
                    packet);
    }
    return;
  }

  detectedColor = received;
  colorSensorAvailable = true;
  colorSensorLastPacketMs = millis();
  colorSensorSerial.print(F("ACK,"));
  colorSensorSerial.print(detectedColor.r);
  colorSensorSerial.print(',');
  colorSensorSerial.print(detectedColor.g);
  colorSensorSerial.print(',');
  colorSensorSerial.println(detectedColor.b);

  static uint32_t validPacketLogMs = 0;
  if (colorSensorLastPacketMs - validPacketLogMs >= 1000) {
    validPacketLogMs = colorSensorLastPacketMs;
    Serial.printf("[COLOR UART] RGB=%u,%u,%u\n",
                  detectedColor.r, detectedColor.g, detectedColor.b);
  }

  if (lightMode == "Colorful") {
    cur = detectedColor;
    if (page == P_LIGHTS && tftMode == M_COLOR) drawColorPanel();
  }
}

void updateColorSensor() {
  static char packet[128];
  static size_t packetLength = 0;
  static bool discardingLongPacket = false;
  static uint32_t noDataLogMs = 0;
  static uint32_t packetLastByteMs = 0;
  static uint32_t incompleteLineLogMs = 0;

  while (colorSensorSerial.available()) {
    const char value = (char)colorSensorSerial.read();
    colorSensorReceiveSeen = true;
    packetLastByteMs = millis();
    if (value == '\n' || value == '\r') {
      if (discardingLongPacket) {
        Serial.println("[COLOR UART] Discarded oversized line (limit 127 bytes)");
      } else {
        packet[packetLength] = '\0';
        if (packetLength > 0) processColorSensorPacket(packet);
      }
      packetLength = 0;
      discardingLongPacket = false;
    } else {
      if (!discardingLongPacket) {
        if (packetLength < sizeof(packet) - 1) packet[packetLength++] = value;
        else {
          packetLength = 0;
          discardingLongPacket = true;
        }
      }
    }
  }

  const uint32_t now = millis();
  if (!colorSensorReceiveSeen && now - noDataLogMs >= 5000) {
    noDataLogMs = now;
    Serial.printf("[COLOR UART] No bytes received on GPIO%d at %d baud; check Nano TX -> ESP32 RX, common GND, and level-shifter direction\n",
                  COLOR_SENSOR_RX_PIN, COLOR_SENSOR_BAUD);
  }
  if (packetLength > 0 && now - packetLastByteMs >= 1000 &&
      now - incompleteLineLogMs >= 5000) {
    incompleteLineLogMs = now;
    Serial.printf("[COLOR UART] Incomplete line (%u bytes); raw HEX:", (unsigned)packetLength);
    const size_t bytesToLog = min(packetLength, (size_t)24);
    for (size_t i = 0; i < bytesToLog; i++) {
      Serial.printf(" %02X", (uint8_t)packet[i]);
    }
    if (packetLength > bytesToLog) Serial.print(" ...");
    Serial.println();
  }
  if (colorSensorAvailable && now - colorSensorLastPacketMs > COLOR_SENSOR_TIMEOUT_MS) {
    colorSensorAvailable = false;
    Serial.println("[COLOR UART] Color data timed out");
  }
}

// Ambient sensing stays local on I2C; only Custom Color uses the UART controller.
constexpr uint8_t VEML7700_ADDR = 0x10;
constexpr uint8_t VEML7700_ALS_CONF = 0x00;
constexpr uint8_t VEML7700_ALS_DATA = 0x04;
constexpr float VEML7700_LUX_PER_COUNT = 0.0576f;

bool writeVemlRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(VEML7700_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(value & 0xFF));
  Wire.write((uint8_t)(value >> 8));
  return Wire.endTransmission() == 0;
}

bool readVemlLux(float* lux) {
  Wire.beginTransmission(VEML7700_ADDR);
  Wire.write(VEML7700_ALS_DATA);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(VEML7700_ADDR, (uint8_t)2) != 2) return false;

  const uint16_t raw = Wire.read() | (Wire.read() << 8);
  *lux = raw * VEML7700_LUX_PER_COUNT;
  return true;
}

void setupAmbientSensor() {
  // Initialize the shared bus; do not probe/read the sensor until Adaptive mode.
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  ambientSensorAvailable = false;
  ambientSensorConfigured = false;
  Serial.printf("[AMBIENT] I2C bus ready (SDA=%d, SCL=%d); sensor reads start in Color Adaptive mode\n",
                I2C_SDA_PIN, I2C_SCL_PIN);
}

void updateAmbientSensor() {
  static uint32_t lastReadMs = 0;
  static uint32_t lastLogMs = 0;
  const uint32_t now = millis();

  if (lightMode != "Color Adaptive") {
    ambientSensorAvailable = false;
    ambientSensorConfigured = false;
    ambientLux = 0.0f;
    return;
  }

  if (now - lastReadMs >= 500) {
    lastReadMs = now;
    if (!ambientSensorConfigured) {
      ambientSensorConfigured = writeVemlRegister(VEML7700_ALS_CONF, 0x0000);
      if (!ambientSensorConfigured && now - lastLogMs >= 1000) {
        lastLogMs = now;
        Serial.println("[AMBIENT] VEML7700 setup failed; check SDA=GPIO32, SCL=GPIO33, power and ground");
      }
      return;
    }

    float measuredLux;
    ambientSensorAvailable = readVemlLux(&measuredLux);
    if (ambientSensorAvailable) {
      ambientLux = measuredLux;
      if (now - lastLogMs >= 1000) {
        lastLogMs = now;
        Serial.printf("[AMBIENT] VEML7700 lux=%.2f, adaptiveBrightness=%d%%\n",
                      ambientLux, brightness);
      }
      if (lightMode == "Color Adaptive") {
        const int targetBrightness = constrain(100 - (int)(ambientLux * 80.0f / 500.0f), 20, 100);
        if (brightness != targetBrightness) {
          brightness = targetBrightness;
          pushZones();
          if (page == P_LIGHTS) drawBrightnessRow();
        }
      }
    } else if (now - lastLogMs >= 1000) {
      lastLogMs = now;
      Serial.println("[AMBIENT] VEML7700 read failed; check SDA=GPIO32, SCL=GPIO33, power and ground");
    }
  }

  if (page == P_LIGHTS && tftMode == M_ADAPT && now - ambientLastDrawMs >= 500) {
    ambientLastDrawMs = now;
    drawModePanel();
  }
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

void drawFocusCue(int x, int y, int w, int h, uint8_t order, bool focused) {
  if (focused) {
    tft.drawRoundRect(x - 3, y - 3, w + 6, h + 6, 7, C_FOCUS);
    tft.drawRoundRect(x - 2, y - 2, w + 4, h + 4, 6, C_FOCUS);
  }

  const int badgeX = x + 10;
  const int badgeY = y + 9;
  tft.fillCircle(badgeX, badgeY, 7, focused ? C_FOCUS : C_PURPLE);
  char number[4];
  snprintf(number, sizeof(number), "%u", order);
  txt(number, badgeX, badgeY, 1, C_WHITE, focused ? C_FOCUS : C_PURPLE, MC_DATUM);
}

void drawEncoderFocusCue(int x, int y, int w, int h, bool focused) {
  if (focused && encoderMode != ENC_NAVIGATE) {
    tft.drawRoundRect(x - 4, y - 4, w + 8, h + 8, 8, C_FOCUS);
  }
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

  // "Exit" tap zone — enlarged button with rounded corners
  if (gateState == GS_OPEN) {
    tft.fillRoundRect(4, 3, 76, HDR_H - 6, 6, C_RED);
    txt("Exit", 42, HDR_H / 2, 2, C_WHITE, C_RED, MC_DATUM);
  }

  // Park name (centred between Exit and tab buttons)
  txt("Silvestre del Moro", 92, HDR_H / 2, 2, C_TEXT, C_HDR, ML_DATUM);

  const char* encoderLabel = encoderMode == ENC_NAVIGATE ? "TAB"
                           : encoderMode == ENC_FOCUS ? "PICK" : "EDIT";
  const uint16_t encoderBadge = encoderMode == ENC_NAVIGATE ? C_GREEN : C_FOCUS;
  tft.fillRoundRect(208, 7, 46, 22, 5, encoderBadge);
  txt(encoderLabel, 231, 18, 1, C_WHITE, encoderBadge, MC_DATUM);

  // Page tabs - 3 tabs now (Lights, Audio, Settings)
  bool aL = (page == P_LIGHTS), aA = (page == P_AUDIO), aS = (page == P_SETTINGS);
  
  tft.fillRect(260, 0, 75, HDR_H - 1, aL ? C_AMBER  : C_HDR);
  txt("Lights", 297, HDR_H / 2, 2, aL ? C_WHITE : C_DIM, aL ? C_AMBER : C_HDR, MC_DATUM);
  
  tft.fillRect(335, 0, 70, HDR_H - 1, aA ? C_GREEN  : C_HDR);
  txt("Audio",  370, HDR_H / 2, 2, aA ? C_WHITE : C_DIM, aA ? C_GREEN : C_HDR, MC_DATUM);
  
  tft.fillRect(405, 0, 75, HDR_H - 1, aS ? C_PURPLE : C_HDR);
  txt("Settings", 442, HDR_H / 2, 2, aS ? C_WHITE : C_DIM, aS ? C_PURPLE : C_HDR, MC_DATUM);

  // WiFi AP dot (green = AP active) + SD dot
  tft.fillCircle(474, 8,  4, WiFi.softAPgetStationNum() >= 0 ? C_GREEN : C_RED);
  tft.fillCircle(474, 22, 4, sdOk ? C_GREEN : C_RED);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FINGERPRINT SENSOR MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

// Load users from SPIFFS
void loadUsers() {
  userCount = 0;
  if (!SPIFFS.exists(USERS_FILE)) {
    Serial.println("[FP] No users file found");
    return;
  }
  
  File file = SPIFFS.open(USERS_FILE, "r");
  if (!file) {
    Serial.println("[FP] Failed to open users file");
    return;
  }
  
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.printf("[FP] JSON parse error: %s\n", error.c_str());
    return;
  }
  
  JsonArray usersArray = doc["users"];
  for (JsonObject user : usersArray) {
    if (userCount >= MAX_USERS) break;
    users[userCount].id = user["id"];
    strlcpy(users[userCount].name, user["name"] | "Unknown", 32);
    users[userCount].active = user["active"] | true;
    userCount++;
  }
  
  Serial.printf("[FP] Loaded %d users\n", userCount);
}

// Save users to SPIFFS
void saveUsers() {
  StaticJsonDocument<4096> doc;
  JsonArray usersArray = doc.createNestedArray("users");
  
  for (int i = 0; i < userCount; i++) {
    JsonObject user = usersArray.createNestedObject();
    user["id"] = users[i].id;
    user["name"] = users[i].name;
    user["active"] = users[i].active;
  }
  
  File file = SPIFFS.open(USERS_FILE, "w");
  if (!file) {
    Serial.println("[FP] Failed to save users");
    return;
  }
  
  serializeJson(doc, file);
  file.close();
  Serial.println("[FP] Users saved");
}

// Find user by fingerprint ID
const char* getUserName(int fpID) {
  for (int i = 0; i < userCount; i++) {
    if (users[i].id == fpID && users[i].active) {
      return users[i].name;
    }
  }
  return "Unknown User";
}

// Find next available fingerprint ID
int findNextFreeID() {
  for (int id = 1; id <= 127; id++) {
    bool used = false;
    for (int i = 0; i < userCount; i++) {
      if (users[i].id == id) {
        used = true;
        break;
      }
    }
    if (!used) return id;
  }
  return -1; // All slots full
}

// Add new user
bool addUser(int fpID, const char* name) {
  if (userCount >= MAX_USERS) return false;
  
  users[userCount].id = fpID;
  strlcpy(users[userCount].name, name, 32);
  users[userCount].active = true;
  userCount++;
  
  saveUsers();
  return true;
}

// Delete user
bool deleteUser(int fpID) {
  for (int i = 0; i < userCount; i++) {
    if (users[i].id == fpID) {
      // Shift remaining users
      for (int j = i; j < userCount - 1; j++) {
        users[j] = users[j + 1];
      }
      userCount--;
      saveUsers();
      
      // Delete from sensor
      if (fpSensorAvailable) {
        finger.deleteModel(fpID);
      }
      
      Serial.printf("[FP] Deleted user ID #%d\n", fpID);
      return true;
    }
  }
  return false;
}

// Probe biometric sensor at given baud and pin combination
bool probeSensor(int baud, int rxPin, int txPin) {
  fingerprintSerial.end();
  delay(60);
  fingerprintSerial.begin(baud, SERIAL_8N1, rxPin, txPin);
  delay(120);
  return finger.verifyPassword();
}

// Probing & initializing optical fingerprint sensor
bool initFingerprintSensor() {
  Serial.println("[FP] Probing optical biometric sensor on UART2...");
  // Sensor boot-up delay
  delay(900);

  // 1. Try standard connection: Sensor TX -> GPIO 16, Sensor RX -> GPIO 17 @ 57600
  if (probeSensor(57600, 16, 17)) {
    fpSensorAvailable = true;
    Serial.println("[FP] ✓ SENSOR CONNECTED! (57600 baud, Sensor TX->GPIO16, Sensor RX->GPIO17)");
    finger.getTemplateCount();
    Serial.printf("[FP] %d template(s) stored in sensor flash\n", finger.templateCount);
    return true;
  }

  // 2. Try standard connection @ 9600 baud
  if (probeSensor(9600, 16, 17)) {
    fpSensorAvailable = true;
    Serial.println("[FP] ✓ SENSOR CONNECTED! (9600 baud, Sensor TX->GPIO16, Sensor RX->GPIO17)");
    finger.getTemplateCount();
    Serial.printf("[FP] %d template(s) stored in sensor flash\n", finger.templateCount);
    return true;
  }

  // 3. Try swapped connection: Sensor TX -> GPIO 17, Sensor RX -> GPIO 16 @ 57600
  if (probeSensor(57600, 17, 16)) {
    fpSensorAvailable = true;
    Serial.println("[FP] ✓ SENSOR CONNECTED! (57600 baud, swapped pins: RX:17, TX:16)");
    finger.getTemplateCount();
    Serial.printf("[FP] %d template(s) stored in sensor flash\n", finger.templateCount);
    return true;
  }

  // 4. Try swapped connection @ 9600 baud
  if (probeSensor(9600, 17, 16)) {
    fpSensorAvailable = true;
    Serial.println("[FP] ✓ SENSOR CONNECTED! (9600 baud, swapped pins: RX:17, TX:16)");
    finger.getTemplateCount();
    Serial.printf("[FP] %d template(s) stored in sensor flash\n", finger.templateCount);
    return true;
  }

  // 5. Try 115200 baud
  if (probeSensor(115200, 16, 17)) {
    fpSensorAvailable = true;
    Serial.println("[FP] ✓ SENSOR CONNECTED! (115200 baud, Sensor TX->GPIO16, Sensor RX->GPIO17)");
    finger.getTemplateCount();
    Serial.printf("[FP] %d template(s) stored in sensor flash\n", finger.templateCount);
    return true;
  }

  fpSensorAvailable = false;
  Serial.println("[FP] ✗ Sensor not found on UART2.");
  Serial.println("[FP]   Wiring guide:");
  Serial.println("[FP]     Sensor TX  --> ESP32 GPIO 16 (RX2)");
  Serial.println("[FP]     Sensor RX  --> ESP32 GPIO 17 (TX2)");
  Serial.println("[FP]     Sensor VCC --> ESP32 5V / VIN (recommended for optical sensors) or 3.3V");
  Serial.println("[FP]     Sensor GND --> ESP32 GND");
  return false;
}

void setupFingerprintSensor() {
  initFingerprintSensor();
}

// Enroll new fingerprint
// Returns: 0=success, 1=no finger, 2=error, 3=mismatch
int enrollFingerprint(int id, int step) {
  if (!fpSensorAvailable) {
    if (!initFingerprintSensor()) return 2;
  }
  
  if (step == 1) {
    // First capture
    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) return 1;
    if (p != FINGERPRINT_OK) return 2;
    
    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) return 2;
    
    Serial.println("[FP] First capture OK");
    return 0;
    
  } else if (step == 2) {
    // Second capture
    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) return 1;
    if (p != FINGERPRINT_OK) return 2;
    
    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) return 2;
    
    // Create model
    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
      Serial.println("[FP] Fingerprints did not match");
      return 3;
    }
    
    // Store model
    p = finger.storeModel(id);
    if (p != FINGERPRINT_OK) return 2;
    
    Serial.printf("[FP] ✓ Enrolled at ID #%d\n", id);
    return 0;
  }
  
  return 2;
}

// Verify fingerprint
// Returns: fingerprint ID if match found, -1 if no match, -2 if error
int verifyFingerprint() {
  if (!fpSensorAvailable) {
    if (!initFingerprintSensor()) return -2;
  }
  
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -2;
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -2;
  
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.printf("[FP] ✓ Match! ID #%d (confidence: %d)\n", 
                  finger.fingerID, finger.confidence);
    return finger.fingerID;
  } else {
    Serial.println("[FP] ✗ No match found");
    return -1;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// GATE SCREEN
// ═══════════════════════════════════════════════════════════════════════════════

void drawGateScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, 4, C_GREEN);

  txt("Silvestre del Moro Park", W / 2, 22, 2, C_DIM, C_BG, MC_DATUM);

  int cx = W / 2, cy = 110;

  switch (gateState) {
    case GS_WAITING: {
      txt("Hi! Welcome!", cx, 100, 4, C_TEXT, C_BG, MC_DATUM);
      txt("Place your finger on the", cx, 145, 2, C_DIM, C_BG, MC_DATUM);
      txt("biometric scanner to control your diorama", cx, 170, 2, C_DIM, C_BG, MC_DATUM);
      
      // Fingerprint icon visual guide
      tft.drawCircle(cx, 230, 35, C_GREEN);
      tft.drawCircle(cx, 230, 28, C_GREEN);
      tft.drawCircle(cx, 230, 21, C_GREEN);
      tft.drawFastVLine(cx, 195, 70, C_GREEN);
      tft.drawFastHLine(cx - 35, 230, 70, C_GREEN);
      
      txt(fpSensorAvailable ? "Sensor Active (TX:17, RX:16) - Touch sensor to scan" : "Touch screen to simulate sensor", cx, 290, 1, C_DIM, C_BG, MC_DATUM);
      break;
    }
    case GS_SCANNING: {
      txt("Scanning Fingerprint...", cx, 110, 4, C_AMBER, C_BG, MC_DATUM);
      txt("Verifying biometric data, please wait...", cx, 150, 2, C_DIM, C_BG, MC_DATUM);

      // Animated fingerprint icon while scanning
      tft.drawCircle(cx, 210, 35, C_AMBER);
      tft.drawCircle(cx, 210, 28, C_AMBER);
      tft.drawCircle(cx, 210, 21, C_AMBER);
      tft.drawFastVLine(cx, 175, 70, C_AMBER);
      tft.drawFastHLine(cx - 35, 210, 70, C_AMBER);

      // Progress bar rail
      tft.fillRoundRect(cx - 120, 270, 240, 12, 6, C_TRACK);
      uint32_t elapsed = millis() - gateMs;
      int fw = constrain((int)(240L * elapsed / SCAN_MS), 0, 240);
      if (fw > 0) tft.fillRoundRect(cx - 120, 270, fw, 12, 6, C_GREEN);
      break;
    }
    case GS_OPEN: {
      // Open padlock — shackle drawn as open arc using segments
      tft.drawCircle(cx, cy - 6, 18, C_GREEN);
      tft.fillRect(cx - 19, cy - 6, 38, 20, C_CARD); // erase lower half of circle
      tft.fillRoundRect(cx - 18, cy + 6, 36, 28, 4, C_GREEN);
      tft.fillCircle(cx, cy + 17, 5, C_WHITE);

      txt("Access Granted!", cx, 178, 4, C_GREEN, C_BG, MC_DATUM);
      txt("You may now control the diorama.", cx, 208, 2, C_DIM, C_BG, MC_DATUM);
      break;
    }
    case GS_GOODBYE: {
      // Closed padlock — full circle shackle
      tft.drawCircle(cx, cy - 6, 18, C_RED);
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
  if (gateState == GS_WAITING) {
    // Poll physical biometric sensor on TX/RX
    static uint32_t lastFpPoll = 0;
    if (millis() - lastFpPoll >= 180) {
      lastFpPoll = millis();
      if (fpSensorAvailable) {
        uint8_t p = finger.getImage();
        if (p == FINGERPRINT_OK) {
          Serial.println("[FP] Finger detected on sensor!");
          enterGateState(GS_SCANNING);
          p = finger.image2Tz();
          if (p == FINGERPRINT_OK) {
            p = finger.fingerSearch();
            if (p == FINGERPRINT_OK) {
              Serial.printf("[FP] Match found! ID #%d (%s)\n", finger.fingerID, getUserName(finger.fingerID));
              enterGateState(GS_OPEN);
              return;
            } else {
              Serial.println("[FP] Fingerprint not recognized");
              tft.fillRect(0, 140, W, 70, C_BG);
              txt("Fingerprint Not Recognized", W / 2, 160, 2, C_RED, C_BG, MC_DATUM);
              txt("Please register or try again", W / 2, 185, 2, C_DIM, C_BG, MC_DATUM);
              delay(1400);
              drawGateScreen();
              enterGateState(GS_WAITING);
              return;
            }
          } else {
            enterGateState(GS_WAITING);
            return;
          }
        }
      }
    }
  } else if (gateState == GS_SCANNING) {
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
//  y  36..76  : Mode tabs  [ Basic ][ Colorful ][ Sound ][ Adaptive ]  (4 tabs)
//  y  80..132 : Power card (on/off toggle + current mode label)
//  y 136..196 : Brightness slider (Basic/Colorful) or read-only indicator (Adaptive)
//  Sound mode hides the brightness row and uses the freed space for sound controls.
//
// ─────────────────────────────────────────────────────────────────────────────

// Mode tabs — 4 tabs, each 116px wide with 3px gap, starting x=4
// 4×116 + 3×3 = 473px — fits in 480
void drawModeTabs() {
  const char* labels[4] = { "Basic", "Colorful", "Sound", "Adaptive" };
  const Mode  modes[4]  = { M_BASIC, M_COLOR, M_SOUND, M_ADAPT };
  for (int i = 0; i < 4; i++) {
    bool act = (tftMode == modes[i]);
    int  bx  = 4 + i * 119;
    btn(bx, BODY_Y + 2, 116, 36, labels[i], act, C_AMBER);
    drawFocusCue(bx, BODY_Y + 2, 116, 36, i + 1, act);
    drawEncoderFocusCue(bx, BODY_Y + 2, 116, 36,
                        encoderControl == ENC_CONTROL_LIGHT_MODE && act);
  }
}

// Power toggle card
void drawPowerCard() {
  int cy = BODY_Y + 42;
  card(10, cy, 460, 48, C_CARD, lightsOn ? C_AMBER : C_BORDER);

  tft.fillCircle(36, cy + 24, 14, lightsOn ? C_AMBER : C_TRACK);
  tft.drawCircle(36, cy + 24,  9, lightsOn ? C_WHITE : C_DIM);
  tft.drawFastVLine(36, cy + 15, 9, lightsOn ? C_WHITE : C_DIM);

  txt(lightsOn ? "Lights  ON" : "Lights  OFF",
      60, cy + 10, 4, C_TEXT, C_CARD, TL_DATUM);
  txt(lightMode.c_str(), 60, cy + 30, 2, C_DIM, C_CARD, TL_DATUM);

  toggleSw(410, cy + 13, lightsOn, C_AMBER);
  drawFocusCue(10, cy, 460, 48, 5, false);
  drawEncoderFocusCue(10, cy, 460, 48, encoderControl == ENC_CONTROL_LIGHT_POWER);
}

// Brightness row. Adaptive mode keeps this as a telemetry-only indicator.
void drawBrightnessRow() {
  if (tftMode == M_SOUND) return;

  int ly = BODY_Y + 100;
  tft.fillRect(0, ly, W, 64, C_BG);
  const bool readOnly = tftMode == M_ADAPT;
  label(readOnly ? "Auto brightness" : "Brightness", SL_X0, ly);
  drawPct(W - 10, ly + 4, brightness, readOnly ? C_DIM : C_AMBER);
  drawSlider(ly + 34, brightness, readOnly ? C_DIM : C_AMBER);
  if (!readOnly) {
    drawFocusCue(SL_X0, ly + 16, SL_X1 - SL_X0, 36, 6, false);
    drawEncoderFocusCue(SL_X0, ly + 16, SL_X1 - SL_X0, 36,
                        encoderControl == ENC_CONTROL_BRIGHTNESS);
  }
}

// ── Colorful panel ────────────────────────────────────────────────────────────
//
//  py+0   : "Color:" swatch and hex, plus live ambient lux
//  py+20  : Hue gradient slider
//  py+44  : "Sat" label + value
//  py+56  : Saturation gradient slider
//  py+76  : Apply-zone buttons  [ALL] [Left] [Right] [Center]
//
void drawColorPanel() {
  int py = BODY_Y + 170;
  tft.fillRect(0, py, W, H - py, C_BG);

  // ── Colour preview + hex ──────────────────────────────────────────────────
  label("Color:", SL_X0, py + 2);
  tft.fillRoundRect(72, py - 2, 36, 18, 4, c565(cur));
  tft.drawRoundRect(72, py - 2, 36, 18, 4, C_TEXT);
  char hex[8]; snprintf(hex, sizeof(hex), "#%02X%02X%02X", cur.r, cur.g, cur.b);
  txt(hex, 114, py + 2, 2, C_TEXT, C_BG, TL_DATUM);

  char sensorLabel[32];
  if (colorSensorAvailable) {
    snprintf(sensorLabel, sizeof(sensorLabel), "Sensor #%02X%02X%02X",
             detectedColor.r, detectedColor.g, detectedColor.b);
  } else strlcpy(sensorLabel, "Color sensor offline", sizeof(sensorLabel));
  txt(sensorLabel, 460, py + 8, 2, colorSensorAvailable ? C_GREEN : C_RED, C_BG, TR_DATUM);

  // ── Hue gradient slider ───────────────────────────────────────────────────
  label("Hue", SL_X0, py + 22);
  for (int x = SL_X0; x < SL_X1; x += 3) {
    int h = (long)(x - SL_X0) * 359 / (SL_X1 - SL_X0);
    RGB c = hsv(h, 100, 100);
    tft.fillRect(x, py + 32, 3, 8, c565(c));
  }
  tft.drawRect(SL_X0, py + 32, SL_X1 - SL_X0, 8, C_BORDER);
  int hkx = SL_X0 + (long)(SL_X1 - SL_X0) * hue / 359;
  tft.fillCircle(hkx, py + 36, 10, c565(hsv(hue, 100, 100)));
  tft.drawCircle(hkx, py + 36, 10, C_TEXT);
  drawFocusCue(SL_X0, py + 22, SL_X1 - SL_X0, 24, 7, false);
  drawEncoderFocusCue(SL_X0, py + 22, SL_X1 - SL_X0, 24,
                      encoderControl == ENC_CONTROL_HUE);

  // ── Saturation gradient slider ────────────────────────────────────────────
  label("Sat", SL_X0, py + 50);
  drawPct(W - 10, py + 50, sat, C_DIM);
  for (int x = SL_X0; x < SL_X1; x += 3) {
    int s = (long)(x - SL_X0) * 100 / (SL_X1 - SL_X0);
    RGB c = hsv(hue, s, 100);
    tft.fillRect(x, py + 60, 3, 8, c565(c));
  }
  tft.drawRect(SL_X0, py + 60, SL_X1 - SL_X0, 8, C_BORDER);
  int skx = SL_X0 + (long)(SL_X1 - SL_X0) * sat / 100;
  tft.fillCircle(skx, py + 64, 10, c565(cur));
  tft.drawCircle(skx, py + 64, 10, C_TEXT);
  drawFocusCue(SL_X0, py + 50, SL_X1 - SL_X0, 24, 8, false);
  drawEncoderFocusCue(SL_X0, py + 50, SL_X1 - SL_X0, 24,
                      encoderControl == ENC_CONTROL_SATURATION);

  // ── Apply-zone buttons ────────────────────────────────────────────────────
  const char* zn[4] = { "ALL", "Left", "Right", "Center" };
  for (int i = 0; i < 4; i++) {
    bool act = (lastApplied == i);
    btn(4 + i * 119, py + 80, 115, 32, zn[i], act, C_AMBER);
    drawFocusCue(4 + i * 119, py + 80, 115, 32, i + 9, false);
    const EncoderControl zoneControls[4] = {
      ENC_CONTROL_COLOR_ALL, ENC_CONTROL_COLOR_LEFT,
      ENC_CONTROL_COLOR_RIGHT, ENC_CONTROL_COLOR_CENTER
    };
    drawEncoderFocusCue(4 + i * 119, py + 80, 115, 32,
                        encoderControl == zoneControls[i]);
  }
}

// Mode-specific lower panel
void drawModePanel() {
  int py = tftMode == M_SOUND ? BODY_Y + 100 : BODY_Y + 168;
  tft.fillRect(0, py, W, H - py, C_BG);

  if (tftMode == M_BASIC) {
    label("Steady white light. Adjust brightness above.", SL_X0, py + 4);
    return;
  }

  if (tftMode == M_COLOR) {
    drawColorPanel();
    return;
  }

  if (tftMode == M_SOUND) {
    label("Sound level", SL_X0, py);
    int bw = (int)(lvl * (SL_X1 - SL_X0));
    tft.fillRect(SL_X0, py + 18, SL_X1 - SL_X0, 20, C_TRACK);
    if (bw > 0) tft.fillRect(SL_X0, py + 18, bw, 20, c565(live));
    label("Sensitivity", SL_X0, py + 50);
    drawPct(W - 10, py + 54, sens, C_AMBER);
    drawSlider(py + 80, sens, C_AMBER);
    drawFocusCue(SL_X0, py + 62, SL_X1 - SL_X0, 36, 7, false);
    drawEncoderFocusCue(SL_X0, py + 62, SL_X1 - SL_X0, 36,
                        encoderControl == ENC_CONTROL_SENSITIVITY);
    return;
  }

  if (tftMode == M_ADAPT) {
    char status[48];
    if (ambientSensorAvailable) {
      snprintf(status, sizeof(status), "Ambient light: %.1f lx", ambientLux);
      label(status, SL_X0, py + 4);
      snprintf(status, sizeof(status), "Adaptive brightness: %d%%", brightness);
      label(status, SL_X0, py + 30);
    } else {
      label("Ambient sensor offline — adaptive dimming paused", SL_X0, py + 4);
    }
    return;
  }
}

void drawLights() {
  tft.fillRect(0, BODY_Y, W, BODY_H, C_BG);
  drawModeTabs();
  drawPowerCard();
  if (tftMode != M_SOUND) drawBrightnessRow();
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
  const char* statusStr = audioPlaying ? "Playing"
                        : audioStarted ? "Paused"
                        : "Stopped";
  uint16_t statusCol = audioPlaying ? C_GREEN : C_DIM;
  txt(statusStr, W / 2, BODY_Y + 70, 2, statusCol, C_BG, MC_DATUM);

  // Transport buttons  (centred block)
  //  [<<]   starts x=30,  w=110
  //  [Play] starts x=185, w=110
  //  [>>]   starts x=340, w=110
  btn( 30, BODY_Y + 92, 110, 58, "<<",                   false,        C_GREEN);
  drawFocusCue(30, BODY_Y + 92, 110, 58, 1, true);
  drawEncoderFocusCue(30, BODY_Y + 92, 110, 58,
                      encoderControl == ENC_CONTROL_AUDIO_PREVIOUS);
  btn(185, BODY_Y + 92, 110, 58, audioPlaying ? "Pause" : "Play",
      audioPlaying, C_GREEN);
  drawFocusCue(185, BODY_Y + 92, 110, 58, 2, false);
  drawEncoderFocusCue(185, BODY_Y + 92, 110, 58,
                      encoderControl == ENC_CONTROL_AUDIO_PLAY);
  btn(340, BODY_Y + 92, 110, 58, ">>",                   false,        C_GREEN);
  drawFocusCue(340, BODY_Y + 92, 110, 58, 3, false);
  drawEncoderFocusCue(340, BODY_Y + 92, 110, 58,
                      encoderControl == ENC_CONTROL_AUDIO_NEXT);

  // Volume
  label("Volume", SL_X0, BODY_Y + 162);
  drawPct(W - 10, BODY_Y + 166, volume, C_GREEN);
  drawSlider(BODY_Y + 200, volume, C_GREEN);
  drawFocusCue(SL_X0, BODY_Y + 182, SL_X1 - SL_X0, 36, 4, false);
  drawEncoderFocusCue(SL_X0, BODY_Y + 182, SL_X1 - SL_X0, 36,
                      encoderControl == ENC_CONTROL_VOLUME);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETTINGS PAGE — Fingerprint Management
// ═══════════════════════════════════════════════════════════════════════════════

void drawSettings() {
  tft.fillRect(0, BODY_Y, W, BODY_H, C_BG);

  // Title
  txt("Fingerprint Management", W / 2, BODY_Y + 14, 4, C_TEXT, C_BG, MC_DATUM);
  
  // Sensor status
  const char* status = fpSensorAvailable ? "Connected" : "Dummy Mode";
  uint16_t statusColor = fpSensorAvailable ? C_GREEN : C_AMBER;
  txt(status, W / 2, BODY_Y + 42, 2, statusColor, C_BG, MC_DATUM);
  
  // Enroll button
  card(40, BODY_Y + 60, 180, 50, C_GREEN, C_GREEN);
  txt("Enroll New", 130, BODY_Y + 85, 2, C_WHITE, C_GREEN, MC_DATUM);
  drawFocusCue(40, BODY_Y + 60, 180, 50, 1, true);
  drawEncoderFocusCue(40, BODY_Y + 60, 180, 50,
                      encoderControl == ENC_CONTROL_SETTINGS_ENROLL);
  
  // List users section
  label("Enrolled Users:", 20, BODY_Y + 125);
  
  if (userCount == 0) {
    txt("No users enrolled yet", W / 2, BODY_Y + 160, 2, C_DIM, C_BG, MC_DATUM);
  } else {
    int y = BODY_Y + 145;
    for (int i = 0; i < min(userCount, 6); i++) {
      // User card
      card(20, y, 440, 28, C_CARD, C_BORDER);
      
      // ID badge
      tft.fillCircle(35, y + 14, 10, C_PURPLE);
      char idStr[4];
      snprintf(idStr, sizeof(idStr), "%d", users[i].id);
      txt(idStr, 35, y + 14, 2, C_WHITE, C_PURPLE, MC_DATUM);
      
      // Name
      txt(users[i].name, 55, y + 14, 2, C_TEXT, C_CARD, ML_DATUM);
      
      // Delete button
      tft.fillRoundRect(410, y + 6, 40, 16, 4, C_RED);
      txt("DEL", 430, y + 14, 1, C_WHITE, C_RED, MC_DATUM);
      drawFocusCue(390, y + 6, 60, 16, i + 2, false);
      drawEncoderFocusCue(390, y + 6, 60, 16,
          encoderControl == static_cast<EncoderControl>(ENC_CONTROL_SETTINGS_DELETE_0 + i));
      
      y += 32;
    }
    
    if (userCount > 6) {
      char more[24];
      snprintf(more, sizeof(more), "+%d more...", userCount - 6);
      txt(more, W / 2, y + 10, 2, C_DIM, C_BG, MC_DATUM);
    }
  }
}

// Enrollment states
enum EnrollState { ENROLL_IDLE, ENROLL_STEP1, ENROLL_STEP1_WAIT, ENROLL_STEP2, ENROLL_STEP2_WAIT, ENROLL_SUCCESS, ENROLL_ERROR };
EnrollState enrollState = ENROLL_IDLE;
int enrollingID = -1;
uint32_t enrollStateMs = 0;

void redrawCurrentPage() {
  drawHeader();
  if (page == P_LIGHTS) drawLights();
  else if (page == P_AUDIO) drawAudio();
  else drawSettings();
}

int getEncoderControls(EncoderControl* controls) {
  int count = 0;
  if (page == P_LIGHTS) {
    controls[count++] = ENC_CONTROL_LIGHT_MODE;
    controls[count++] = ENC_CONTROL_LIGHT_POWER;
    if (tftMode != M_SOUND && tftMode != M_ADAPT) {
      controls[count++] = ENC_CONTROL_BRIGHTNESS;
    }
    if (tftMode == M_COLOR) {
      controls[count++] = ENC_CONTROL_HUE;
      controls[count++] = ENC_CONTROL_SATURATION;
      controls[count++] = ENC_CONTROL_COLOR_ALL;
      controls[count++] = ENC_CONTROL_COLOR_LEFT;
      controls[count++] = ENC_CONTROL_COLOR_RIGHT;
      controls[count++] = ENC_CONTROL_COLOR_CENTER;
    } else if (tftMode == M_SOUND) {
      controls[count++] = ENC_CONTROL_SENSITIVITY;
    }
  } else if (page == P_AUDIO) {
    controls[count++] = ENC_CONTROL_AUDIO_PREVIOUS;
    controls[count++] = ENC_CONTROL_AUDIO_PLAY;
    controls[count++] = ENC_CONTROL_AUDIO_NEXT;
    controls[count++] = ENC_CONTROL_VOLUME;
  } else {
    controls[count++] = ENC_CONTROL_SETTINGS_ENROLL;
    for (int i = 0; i < min(userCount, 6); i++) {
      controls[count++] = static_cast<EncoderControl>(ENC_CONTROL_SETTINGS_DELETE_0 + i);
    }
  }
  return count;
}

void syncEncoderControlIndex() {
  EncoderControl controls[12];
  const int count = getEncoderControls(controls);
  for (int i = 0; i < count; i++) {
    if (controls[i] == encoderControl) {
      encoderControlIndex = i;
      return;
    }
  }
  encoderControlIndex = 0;
  if (count > 0) encoderControl = controls[0];
}

void moveEncoderFocus(int direction) {
  EncoderControl controls[12];
  const int count = getEncoderControls(controls);
  if (count == 0) return;
  syncEncoderControlIndex();
  encoderControlIndex = (encoderControlIndex + direction + count) % count;
  encoderControl = controls[encoderControlIndex];
  redrawCurrentPage();
}

bool encoderControlCanAdjust() {
  switch (encoderControl) {
    case ENC_CONTROL_LIGHT_MODE:
    case ENC_CONTROL_BRIGHTNESS:
    case ENC_CONTROL_HUE:
    case ENC_CONTROL_SATURATION:
    case ENC_CONTROL_SENSITIVITY:
    case ENC_CONTROL_VOLUME:
      return true;
    default:
      return false;
  }
}

void selectEncoderControl(EncoderControl control) {
  encoderControl = control;
  syncEncoderControlIndex();
  encoderMode = encoderControlCanAdjust() ? ENC_ADJUST : ENC_FOCUS;
  drawHeader();
}

void applyColorZone(uint8_t zoneIndex) {
  if (zoneIndex > 3) return;
  if (zoneIndex == 0) zone[0] = zone[1] = zone[2] = cur;
  else zone[zoneIndex - 1] = cur;
  lastApplied = zoneIndex;
  lightsOn = true;
  pushZones();
  const char* zoneNames[4] = { "ALL", "Left", "Right", "Center" };
  Serial.printf("[COLOR] Applied #%02X%02X%02X to %s\n",
                cur.r, cur.g, cur.b, zoneNames[zoneIndex]);
  drawColorPanel();
}

void activateEncoderControl() {
  switch (encoderControl) {
    case ENC_CONTROL_LIGHT_POWER:
      lightsOn = !lightsOn;
      pushZones();
      Serial.printf("[LIGHTS] %s via encoder\n", lightsOn ? "ON" : "OFF");
      drawPowerCard();
      break;
    case ENC_CONTROL_AUDIO_PREVIOUS:
      stepTrack(-1);
      if (page == P_AUDIO) drawAudio();
      break;
    case ENC_CONTROL_AUDIO_PLAY:
      togglePlay();
      if (page == P_AUDIO) drawAudio();
      break;
    case ENC_CONTROL_AUDIO_NEXT:
      stepTrack(1);
      if (page == P_AUDIO) drawAudio();
      break;
    case ENC_CONTROL_COLOR_ALL:
      applyColorZone(0);
      break;
    case ENC_CONTROL_COLOR_LEFT:
      applyColorZone(1);
      break;
    case ENC_CONTROL_COLOR_RIGHT:
      applyColorZone(2);
      break;
    case ENC_CONTROL_COLOR_CENTER:
      applyColorZone(3);
      break;
    case ENC_CONTROL_SETTINGS_ENROLL:
      startEnrollment();
      break;
    default:
      if (encoderControl >= ENC_CONTROL_SETTINGS_DELETE_0 &&
          encoderControl <= ENC_CONTROL_SETTINGS_DELETE_5) {
        const int index = encoderControl - ENC_CONTROL_SETTINGS_DELETE_0;
        if (index < userCount) deleteUser(users[index].id);
        syncEncoderControlIndex();
        redrawCurrentPage();
      }
      break;
  }
}

void changePage(Page nextPage, int direction) {
  if (nextPage == page) return;

  const int stripWidth = 40;
  for (int i = 0; i < W / stripWidth; i++) {
    const int x = direction > 0 ? i * stripWidth : W - (i + 1) * stripWidth;
    tft.fillRect(x, 0, stripWidth, H, C_BG);
    delay(6);
  }

  page = nextPage;
  encoderMode = ENC_NAVIGATE;
  if (page == P_LIGHTS) encoderControl = ENC_CONTROL_BRIGHTNESS;
  else if (page == P_AUDIO) encoderControl = ENC_CONTROL_VOLUME;
  else encoderControl = ENC_CONTROL_SETTINGS_ENROLL;
  syncEncoderControlIndex();
  redrawCurrentPage();
}

void setupEncoder() {
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  encoderPreviousState = (digitalRead(ENCODER_CLK_PIN) << 1) |
                         digitalRead(ENCODER_DT_PIN);
  encoderButtonStableHigh = digitalRead(ENCODER_SW_PIN) == HIGH;
  encoderButtonCandidateHigh = encoderButtonStableHigh;
  encoderButtonChangeMs = millis();
}

void applyEncoderStep(int direction) {
  if (encoderMode == ENC_NAVIGATE) {
    const int next = (static_cast<int>(page) + direction + 3) % 3;
    changePage(static_cast<Page>(next), direction);
    return;
  }
  if (encoderMode == ENC_FOCUS) {
    moveEncoderFocus(direction);
    return;
  }

  switch (encoderControl) {
    case ENC_CONTROL_LIGHT_MODE: {
      tftMode = static_cast<Mode>((static_cast<int>(tftMode) + direction + 4) % 4);
      if (tftMode == M_BASIC) {
        lightMode = "Basic";
        setAll({255, 255, 255});
      } else if (tftMode == M_COLOR) {
        lightMode = "Colorful";
      } else if (tftMode == M_SOUND) {
        lightMode = "Sound Reactive";
      } else {
        lightMode = "Color Adaptive";
      }
      Serial.printf("[ENCODER] Lighting mode: %s\n", lightMode.c_str());
      drawLights();
      break;
    }

    case ENC_CONTROL_LIGHT_POWER:
      lightsOn = direction > 0;
      pushZones();
      drawPowerCard();
      break;

    case ENC_CONTROL_BRIGHTNESS:
      if (tftMode == M_SOUND || tftMode == M_ADAPT) break;
      brightness = constrain(brightness + direction * 5, 0, 100);
      pushZones();
      drawBrightnessRow();
      break;

    case ENC_CONTROL_HUE:
      hue = constrain(hue + direction * 6, 0, 359);
      cur = hsv(hue, sat, 100);
      drawColorPanel();
      break;

    case ENC_CONTROL_SATURATION:
      sat = constrain(sat + direction * 5, 0, 100);
      cur = hsv(hue, sat, 100);
      drawColorPanel();
      break;

    case ENC_CONTROL_SENSITIVITY: {
      sens = constrain(sens + direction * 5, 0, 100);
      drawModePanel();
      break;
    }

    case ENC_CONTROL_VOLUME:
      volume = constrain(volume + direction * 5, 0, 100);
      if (SD_AUDIO_ENABLED) audio.setVolume(volToAudio(volume));
      if (page == P_AUDIO) drawAudio();
      break;

    default:
      break;
  }
}

void handleEncoderShortPress() {
  if (encoderMode == ENC_NAVIGATE) {
    syncEncoderControlIndex();
    encoderMode = ENC_FOCUS;
  } else if (encoderMode == ENC_FOCUS) {
    if (encoderControlCanAdjust()) encoderMode = ENC_ADJUST;
    else activateEncoderControl();
  } else {
    encoderMode = ENC_FOCUS;
  }
  Serial.printf("[ENCODER] Mode: %s\n",
                encoderMode == ENC_NAVIGATE ? "tab navigation"
                : encoderMode == ENC_FOCUS ? "control selection" : "value adjustment");
  if (enrollState == ENROLL_IDLE) redrawCurrentPage();
}

void pollEncoder() {
  static const int8_t transitions[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
  };

  const uint8_t currentState = (digitalRead(ENCODER_CLK_PIN) << 1) |
                               digitalRead(ENCODER_DT_PIN);
  const uint8_t transition = (encoderPreviousState << 2) | currentState;
  if ((encoderPreviousState ^ currentState) == 0x03) {
    encoderQuarterSteps = 0;
  } else if (gateState == GS_OPEN && enrollState == ENROLL_IDLE) {
    encoderQuarterSteps += transitions[transition];
  } else {
    encoderQuarterSteps = 0;
  }
  encoderPreviousState = currentState;

  if (gateState == GS_OPEN && enrollState == ENROLL_IDLE &&
      encoderQuarterSteps >= 4) {
    encoderQuarterSteps = 0;
    applyEncoderStep(1);
  } else if (gateState == GS_OPEN && enrollState == ENROLL_IDLE &&
             encoderQuarterSteps <= -4) {
    encoderQuarterSteps = 0;
    applyEncoderStep(-1);
  }

  const uint32_t now = millis();
  const bool buttonHigh = digitalRead(ENCODER_SW_PIN) == HIGH;
  if (buttonHigh != encoderButtonCandidateHigh) {
    encoderButtonCandidateHigh = buttonHigh;
    encoderButtonChangeMs = now;
  }

  if (encoderButtonCandidateHigh != encoderButtonStableHigh &&
      now - encoderButtonChangeMs >= 30) {
    encoderButtonStableHigh = encoderButtonCandidateHigh;
    if (!encoderButtonStableHigh && gateState == GS_OPEN &&
        enrollState == ENROLL_IDLE) {
      encoderButtonDownMs = now;
      encoderLongPressHandled = false;
    } else if (encoderButtonStableHigh && gateState == GS_OPEN &&
               enrollState == ENROLL_IDLE && !encoderLongPressHandled) {
      handleEncoderShortPress();
    }
  }

  if (!encoderButtonStableHigh && gateState == GS_OPEN &&
      enrollState == ENROLL_IDLE && !encoderLongPressHandled &&
      now - encoderButtonDownMs >= 700) {
    encoderLongPressHandled = true;
    encoderMode = ENC_NAVIGATE;
    Serial.println("[ENCODER] Mode: tab navigation");
    redrawCurrentPage();
  }
}

void drawEnrollmentScreen() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, 4, C_PURPLE);
  
  txt("Fingerprint Enrollment", W / 2, 22, 2, C_DIM, C_BG, MC_DATUM);
  
  int cx = W / 2;
  
  switch (enrollState) {
    case ENROLL_STEP1:
      txt("Step 1 of 2", cx, 80, 2, C_PURPLE, C_BG, MC_DATUM);
      txt("Place finger on sensor", cx, 120, 4, C_TEXT, C_BG, MC_DATUM);
      
      // Fingerprint icon
      tft.drawCircle(cx, 180, 35, C_PURPLE);
      tft.drawCircle(cx, 180, 28, C_PURPLE);
      tft.drawCircle(cx, 180, 21, C_PURPLE);
      tft.drawFastVLine(cx, 145, 70, C_PURPLE);
      tft.drawFastHLine(cx - 35, 180, 70, C_PURPLE);
      
      card(cx - 80, 250, 160, 40, C_BORDER, C_BORDER);
      txt("Cancel", cx, 270, 2, C_DIM, C_BG, MC_DATUM);
      break;
      
    case ENROLL_STEP1_WAIT:
      txt("Remove finger", cx, 120, 4, C_GREEN, C_BG, MC_DATUM);
      txt("Preparing for second scan...", cx, 160, 2, C_DIM, C_BG, MC_DATUM);
      break;
      
    case ENROLL_STEP2:
      txt("Step 2 of 2", cx, 80, 2, C_PURPLE, C_BG, MC_DATUM);
      txt("Place SAME finger again", cx, 120, 4, C_TEXT, C_BG, MC_DATUM);
      
      // Fingerprint icon
      tft.drawCircle(cx, 180, 35, C_PURPLE);
      tft.drawCircle(cx, 180, 28, C_PURPLE);
      tft.drawCircle(cx, 180, 21, C_PURPLE);
      tft.drawFastVLine(cx, 145, 70, C_PURPLE);
      tft.drawFastHLine(cx - 35, 180, 70, C_PURPLE);
      
      card(cx - 80, 250, 160, 40, C_BORDER, C_BORDER);
      txt("Cancel", cx, 270, 2, C_DIM, C_BG, MC_DATUM);
      break;
      
    case ENROLL_SUCCESS:
      tft.fillCircle(cx, 140, 40, C_GREEN);
      txt("✓", cx, 140, 7, C_WHITE, C_GREEN, MC_DATUM);
      txt("Success!", cx, 200, 4, C_GREEN, C_BG, MC_DATUM);
      txt("Fingerprint enrolled", cx, 235, 2, C_TEXT, C_BG, MC_DATUM);
      break;
      
    case ENROLL_ERROR:
      tft.fillCircle(cx, 140, 40, C_RED);
      txt("✗", cx, 140, 7, C_WHITE, C_RED, MC_DATUM);
      txt("Error", cx, 200, 4, C_RED, C_BG, MC_DATUM);
      txt("Fingerprints did not match", cx, 235, 2, C_TEXT, C_BG, MC_DATUM);
      
      card(cx - 80, 260, 160, 40, C_PURPLE, C_PURPLE);
      txt("Try Again", cx, 280, 2, C_WHITE, C_PURPLE, MC_DATUM);
      break;
      
    default:
      break;
  }
}

void startEnrollment() {
  enrollingID = findNextFreeID();
  if (enrollingID == -1) {
    Serial.println("[FP] All slots full!");
    return;
  }
  
  enrollState = ENROLL_STEP1;
  enrollStateMs = millis();
  drawEnrollmentScreen();
  Serial.printf("[FP] Starting enrollment for ID #%d\n", enrollingID);
}

void pollEnrollment() {
  if (enrollState == ENROLL_IDLE) return;
  
  if (enrollState == ENROLL_STEP1) {
    int result = enrollFingerprint(enrollingID, 1);
    if (result == 0) {
      // Success - move to wait state
      enrollState = ENROLL_STEP1_WAIT;
      enrollStateMs = millis();
      drawEnrollmentScreen();
    } else if (result == 2) {
      // Error
      enrollState = ENROLL_ERROR;
      drawEnrollmentScreen();
    }
    
  } else if (enrollState == ENROLL_STEP1_WAIT) {
    if (millis() - enrollStateMs > 2000) {
      enrollState = ENROLL_STEP2;
      drawEnrollmentScreen();
    }
    
  } else if (enrollState == ENROLL_STEP2) {
    int result = enrollFingerprint(enrollingID, 2);
    if (result == 0) {
      // Success!
      char newName[32];
      snprintf(newName, sizeof(newName), "User %d", enrollingID);
      addUser(enrollingID, newName);
      
      enrollState = ENROLL_SUCCESS;
      drawEnrollmentScreen();
      
      // Auto return to settings after 2s
      enrollStateMs = millis();
      
    } else if (result == 2 || result == 3) {
      // Error or mismatch
      enrollState = ENROLL_ERROR;
      drawEnrollmentScreen();
    }
    
  } else if (enrollState == ENROLL_SUCCESS) {
    if (millis() - enrollStateMs > 2000) {
      enrollState = ENROLL_IDLE;
      page = P_SETTINGS;
      drawHeader();
      drawSettings();
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SOUND SENSOR
// ═══════════════════════════════════════════════════════════════════════════════

void updateMicSensor() {
  const uint32_t now = millis();
  if (lightMode != "Sound Reactive") {
    micRawMin = 0;
    micRawMax = 0;
    micPeakToPeak = 0;
    micRms = 0;
    micLevel = 0;
    micDetected = false;
    lvl = 0;
    return;
  }

  if (now - micLastSampleMs < 40) return;
  micLastSampleMs = now;

  constexpr int SAMPLE_COUNT = 96;
  int samples[SAMPLE_COUNT];
  uint32_t sum = 0;
  micRawMin = 4095;
  micRawMax = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    const int sample = analogRead(MIC_PIN);
    samples[i] = sample;
    sum += sample;
    if (sample < micRawMin) micRawMin = sample;
    if (sample > micRawMax) micRawMax = sample;
    delayMicroseconds(125);
  }

  const int mean = sum / SAMPLE_COUNT;
  uint64_t squaredErrorSum = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    const int centered = samples[i] - mean;
    squaredErrorSum += (uint64_t)(centered * centered);
  }

  micPeakToPeak = micRawMax - micRawMin;
  micRms = (int)sqrtf((float)squaredErrorSum / SAMPLE_COUNT);
  micLevel = constrain(map(micPeakToPeak, 0, 900, 0, 1023), 0, 1023);

  const int detectionThreshold = max(8, 90 - sens * 7 / 10);
  micDetected = micRms >= detectionThreshold;

  if (now - micLastLogMs >= 500) {
    micLastLogMs = now;
    Serial.printf("[SOUND] GPIO34 min=%d max=%d p-p=%d rms=%d level=%d/1023 detected=%s threshold=%d\n",
                  micRawMin, micRawMax, micPeakToPeak, micRms, micLevel,
                  micDetected ? "yes" : "no", detectionThreshold);
  }
}

void updateLive() {
  static uint32_t last = 0;
  if (millis() - last < 40) return;
  last = millis();

  if (lightMode == "Sound Reactive" && lightsOn) {
    if (micDetected && micLevel > 0) {
      lvl     = max(micLevel / 1023.0f, lvl * 0.82f);
      hueBase = (hueBase + 2) % 360;
      live    = hsv((hueBase + (int)(lvl * 120)) % 360, 100, 12 + (int)(88 * lvl));
      setAll(live);
    } else {
      lvl = 0;
    }
    // Refresh level bar if on lights page
    if (page == P_LIGHTS && tftMode == M_SOUND) {
      int py = BODY_Y + 100;
      int bw = (int)(lvl * (SL_X1 - SL_X0));
      tft.fillRect(SL_X0, py + 18, SL_X1 - SL_X0, 20, C_TRACK);
      if (bw > 0) tft.fillRect(SL_X0, py + 18, bw, 20, c565(live));
    }
  }

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
  Serial.printf("[TOUCH] Press x=%d y=%d gate=%d page=%d\n",
                x, y, (int)gateState, (int)page);

  // ── Gate screen (dummy biometric simulation) ────────────────────────────
  if (gateState != GS_OPEN) {
    // Any touch on the waiting screen simulates biometric sensor placement
    if (gateState == GS_WAITING) {
      enterGateState(GS_SCANNING);
    }
    return;
  }

  // ── Header taps ──────────────────────────────────────────────────────────
  if (y < HDR_H + 12) {
    // Exit tap zone (enlarged hit box: 0..95 px)
    if (x < 95) {
      enterGateState(GS_GOODBYE);
      return;
    }
    // Page tabs - 3 tabs now
    if (inRect(x, y, 250, 0, 85, HDR_H + 10)) {
      const int direction = ((static_cast<int>(P_LIGHTS) - static_cast<int>(page) + 3) % 3 == 1) ? 1 : -1;
      changePage(P_LIGHTS, direction);
      return;
    }
    if (inRect(x, y, 330, 0, 75, HDR_H + 10)) {
      const int direction = ((static_cast<int>(P_AUDIO) - static_cast<int>(page) + 3) % 3 == 1) ? 1 : -1;
      changePage(P_AUDIO, direction);
      return;
    }
    if (inRect(x, y, 400, 0, 80, HDR_H + 10)) {
      const int direction = ((static_cast<int>(P_SETTINGS) - static_cast<int>(page) + 3) % 3 == 1) ? 1 : -1;
      changePage(P_SETTINGS, direction);
      return;
    }
    return;
  }

  // ── Enrollment screen ──────────────────────────────────────────────────────
  if (enrollState != ENROLL_IDLE) {
    int cx = W / 2;
    
    // Cancel button during enrollment
    if ((enrollState == ENROLL_STEP1 || enrollState == ENROLL_STEP2) &&
        inRect(x, y, cx - 80, 250, 160, 40)) {
      enrollState = ENROLL_IDLE;
      page = P_SETTINGS;
      drawHeader();
      drawSettings();
      Serial.println("[FP] Enrollment cancelled");
      return;
    }
    
    // Try Again button on error
    if (enrollState == ENROLL_ERROR && inRect(x, y, cx - 80, 260, 160, 40)) {
      startEnrollment();
      return;
    }
    
    return; // Block all other touches during enrollment
  }

  // ── Settings page ──────────────────────────────────────────────────────────
  if (page == P_SETTINGS) {
    // Enroll New button
    if (inRect(x, y, 40, BODY_Y + 60, 180, 50)) {
      selectEncoderControl(ENC_CONTROL_SETTINGS_ENROLL);
      startEnrollment();
      return;
    }
    
    // Delete user buttons
    if (userCount > 0) {
      int cy = BODY_Y + 145;
      for (int i = 0; i < min(userCount, 6); i++) {
        if (inRect(x, y, 410, cy + 6, 40, 16)) {
          // Delete this user
          const int deletedID = users[i].id;
          selectEncoderControl(static_cast<EncoderControl>(ENC_CONTROL_SETTINGS_DELETE_0 + i));
          deleteUser(deletedID);
          syncEncoderControlIndex();
          drawSettings();
          Serial.printf("[FP] Deleted user #%d via TFT\n", deletedID);
          return;
        }
        cy += 32;
      }
    }
    return;
  }

  // ── Audio page ────────────────────────────────────────────────────────────
  if (page == P_AUDIO) {
    // <<
    if (inRect(x, y, 30,  BODY_Y + 92, 110, 58)) {
      selectEncoderControl(ENC_CONTROL_AUDIO_PREVIOUS);
      stepTrack(-1); drawAudio(); return;
    }
    // Play/Pause
    if (inRect(x, y, 185, BODY_Y + 92, 110, 58)) {
      selectEncoderControl(ENC_CONTROL_AUDIO_PLAY);
      togglePlay(); drawAudio(); return;
    }
    // >>
    if (inRect(x, y, 340, BODY_Y + 92, 110, 58)) {
      selectEncoderControl(ENC_CONTROL_AUDIO_NEXT);
      stepTrack(1); drawAudio(); return;
    }
    // Volume slider zone
    if (inRect(x, y, SL_X0, BODY_Y + 182, SL_X1 - SL_X0, 40)) {
      selectEncoderControl(ENC_CONTROL_VOLUME);
      drawAudio();
      dragging = D_VOL; onDrag(x);
    }
    return;
  }

  // ── Lights page ───────────────────────────────────────────────────────────
  if (page == P_LIGHTS) {
    // Mode tabs row  y = BODY_Y+2 .. BODY_Y+38
    if (inRect(x, y, 0, BODY_Y, W, 40)) {
      const Mode modes[4] = { M_BASIC, M_COLOR, M_SOUND, M_ADAPT };
      for (int i = 0; i < 4; i++) {
        if (inRect(x, y, 4 + i * 119, BODY_Y + 2, 116, 36)) {
          selectEncoderControl(ENC_CONTROL_LIGHT_MODE);
          if (tftMode != modes[i]) {
            tftMode = modes[i];
            if (tftMode == M_BASIC)  { lightMode = "Basic";          setAll({255,255,255}); }
            if (tftMode == M_COLOR)  { lightMode = "Colorful"; }
            if (tftMode == M_SOUND)  { lightMode = "Sound Reactive"; }
            if (tftMode == M_ADAPT)  { lightMode = "Color Adaptive"; }
            Serial.printf("[MODE] %s\n", lightMode.c_str());
            drawLights();
          } else {
            drawModeTabs();
          }
          return;
        }
      }
    }

    // Power toggle card  y = BODY_Y+42 .. BODY_Y+90
    if (inRect(x, y, 10, BODY_Y + 42, 460, 48)) {
      selectEncoderControl(ENC_CONTROL_LIGHT_POWER);
      lightsOn = !lightsOn;
      Serial.printf("[LIGHTS] %s\n", lightsOn ? "ON" : "OFF");
      pushZones();
      drawPowerCard();
      return;
    }

    // Brightness is adjustable only in Basic and Colorful modes.
    if (tftMode != M_SOUND && tftMode != M_ADAPT &&
        inRect(x, y, SL_X0, BODY_Y + 116, SL_X1 - SL_X0, 52)) {
      selectEncoderControl(ENC_CONTROL_BRIGHTNESS);
      drawBrightnessRow();
      dragging = D_BR; onDrag(x); return;
    }

    // ── Colorful mode touch zones ─────────────────────────────────────────
    if (tftMode == M_COLOR) {
      int py = BODY_Y + 170;

      // Hue slider zone  y = py+24 .. py+48
      if (inRect(x, y, SL_X0, py + 24, SL_X1 - SL_X0, 24)) {
        selectEncoderControl(ENC_CONTROL_HUE);
        drawColorPanel();
        dragging = D_HUE; onDrag(x); return;
      }

      // Saturation slider zone  y = py+54 .. py+78
      if (inRect(x, y, SL_X0, py + 54, SL_X1 - SL_X0, 24)) {
        selectEncoderControl(ENC_CONTROL_SATURATION);
        drawColorPanel();
        dragging = D_SAT; onDrag(x); return;
      }

      // Apply-zone buttons  y = py+80 .. py+112
      if (inRect(x, y, 0, py + 80, W, 34)) {
        for (int i = 0; i < 4; i++) {
          if (inRect(x, y, 4 + i * 119, py + 80, 115, 32)) {
            const EncoderControl zoneControls[4] = {
              ENC_CONTROL_COLOR_ALL, ENC_CONTROL_COLOR_LEFT,
              ENC_CONTROL_COLOR_RIGHT, ENC_CONTROL_COLOR_CENTER
            };
            selectEncoderControl(zoneControls[i]);
            applyColorZone(i);
            return;
          }
        }
      }
      return;
    }

    // Sensitivity slider (Sound mode only)  y = py+62 .. py+118
    if (tftMode == M_SOUND) {
      int py = BODY_Y + 100;
      if (inRect(x, y, SL_X0, py + 62, SL_X1 - SL_X0, 56)) {
        selectEncoderControl(ENC_CONTROL_SENSITIVITY);
        drawModePanel();
        dragging = D_SENS; onDrag(x); return;
      }
    }
  }
}

void onDrag(int x) {
  int p = pctFromX(x);
  switch (dragging) {
    case D_BR:
      if (tftMode == M_SOUND || tftMode == M_ADAPT) {
        dragging = D_NONE;
        break;
      }
      if (p != brightness) {
        brightness = p;
        Serial.printf("[LIGHTS] Brightness %d%%\n", brightness);
        drawPct(W - 10, BODY_Y + 104, p, C_AMBER);
        drawSlider(BODY_Y + 134, p, C_AMBER);
        pushZones();
      }
      break;

    case D_HUE: {
      int h = (long)p * 359 / 100;
      if (h != hue) {
        hue = h;
        cur = hsv(hue, sat, 100);
        Serial.printf("[COLOR] Hue %d -> #%02X%02X%02X\n", hue, cur.r, cur.g, cur.b);
        drawColorPanel();
      }
      break;
    }

    case D_SAT:
      if (p != sat) {
        sat = p;
        cur = hsv(hue, sat, 100);
        Serial.printf("[COLOR] Sat %d%% -> #%02X%02X%02X\n", sat, cur.r, cur.g, cur.b);
        drawColorPanel();
      }
      break;

    case D_SENS:
      if (p != sens) {
        sens = p;
        Serial.printf("[SOUND] Sensitivity %d%%\n", sens);
        drawModePanel();
      }
      break;

    case D_VOL:
      if (p != volume) {
        volume = p;
        Serial.printf("[AUDIO] Volume %d%%\n", volume);
        drawPct(W - 10, BODY_Y + 166, p, C_GREEN);
        drawSlider(BODY_Y + 200, p, C_GREEN);
        drawEncoderFocusCue(SL_X0, BODY_Y + 182, SL_X1 - SL_X0, 36,
                            encoderControl == ENC_CONTROL_VOLUME);
        if (SD_AUDIO_ENABLED) audio.setVolume(volToAudio(p));
      }
      break;

    default: break;
  }
}

bool readCalibratedTouch(uint16_t* x, uint16_t* y) {
  static uint32_t invalidRawSinceMs = 0;
  static uint32_t badRawLogMs = 0;

  uint16_t pressure = tft.getTouchRawZ();
  if (pressure <= 350) {
    invalidRawSinceMs = 0;
    return false;
  }

  delayMicroseconds(1000);
  pressure = tft.getTouchRawZ();
  if (pressure <= 350) {
    invalidRawSinceMs = 0;
    return false;
  }

  uint16_t rawX, rawY;
  tft.getTouchRaw(&rawX, &rawY);
  if (rawX == 0 || rawY == 0 || rawX >= 4095 || rawY >= 4095) {
    const uint32_t now = millis();
    if (invalidRawSinceMs == 0) invalidRawSinceMs = now;
    if (now - invalidRawSinceMs >= 1000 && now - badRawLogMs >= 5000) {
      badRawLogMs = millis();
      Serial.printf("[TOUCH] Sustained invalid samples x=%u y=%u; check TFT SDO is disconnected and XPT T_DO is on GPIO 19\n",
                    rawX, rawY);
    }
    return false;
  }
  invalidRawSinceMs = 0;

  const int32_t x0 = touchCalData[0];
  const int32_t xSpan = touchCalData[1];
  const int32_t y0 = touchCalData[2];
  const int32_t ySpan = touchCalData[3];
  if (!touchCalibrationReady || xSpan == 0 || ySpan == 0) return false;

  int32_t screenX, screenY;
  if (touchCalData[4] & 0x01) {
    screenX = (rawY - x0) * (W - 1) / xSpan;
    screenY = (rawX - y0) * (H - 1) / ySpan;
  } else {
    screenX = (rawX - x0) * (W - 1) / xSpan;
    screenY = (rawY - y0) * (H - 1) / ySpan;
  }

  if (touchCalData[4] & 0x02) screenX = (W - 1) - screenX;
  if (touchCalData[4] & 0x04) screenY = (H - 1) - screenY;
  if (screenX < 0 || screenX >= W || screenY < 0 || screenY >= H) {
    static uint32_t outOfRangeLogMs = 0;
    if (millis() - outOfRangeLogMs >= 500) {
      outOfRangeLogMs = millis();
      Serial.printf("[TOUCH] Raw (%u,%u) maps outside screen to (%ld,%ld); calibration invalid\n",
                    rawX, rawY, (long)screenX, (long)screenY);
    }
    return false;
  }

  *x = screenX;
  *y = screenY;
  static uint32_t touchLogMs = 0;
  if (millis() - touchLogMs >= 300) {
    touchLogMs = millis();
    Serial.printf("[TOUCH] Mapped x=%u y=%u pressure=%u\n", *x, *y, pressure);
  }
  return true;
}

void pollTouch() {
  static uint8_t miss = 0;
  uint16_t x, y;
  if (readCalibratedTouch(&x, &y)) {
    miss = 0;
    if (!wasTouched) { wasTouched = true; onPress(x, y); }
    else if (dragging) onDrag(x);
  } else {
    if (wasTouched && ++miss >= 3) {
      wasTouched = false; miss = 0;
      dragging = D_NONE;
    }

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
    if (lightMode == "Basic")                tftMode = M_BASIC;
    else if (lightMode == "Colorful")        tftMode = M_COLOR;
    else if (lightMode == "Sound Reactive")  tftMode = M_SOUND;
    else if (lightMode == "Color Adaptive")  tftMode = M_ADAPT;
    else                                     tftMode = M_BASIC;
    if (tftMode == M_BASIC) setAll({255, 255, 255});
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
  if (!SD_AUDIO_ENABLED) {
    server.send(503, "application/json", "{\"error\":\"audio is temporarily disabled\"}");
    return;
  }
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
  if (!SD_AUDIO_ENABLED) {
    server.send(503, "application/json", "{\"error\":\"audio is temporarily disabled\"}");
    return;
  }
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
  if (!SD_AUDIO_ENABLED) {
    server.send(503, "application/json", "{\"error\":\"audio is temporarily disabled\"}");
    return;
  }
  audio.stopSong();
  audioPlaying = audioStarted = false;
  if (page == P_AUDIO && gateState == GS_OPEN) drawAudio();
  jsonOk();
}

// ── GET /api/audio/volume?volume=0-100 ───────────────────────────────────────
void handleAudioVolume() {
  if (!SD_AUDIO_ENABLED) {
    server.send(503, "application/json", "{\"error\":\"audio is temporarily disabled\"}");
    return;
  }
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
  if (!SD_AUDIO_ENABLED) {
    server.send(200, "application/json", "{\"files\":[]}");
    return;
  }
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
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"detected\":%s,\"level\":%d,\"rms\":%d,\"peakToPeak\":%d,\"r\":%u,\"g\":%u,\"b\":%u}",
           micDetected ? "true" : "false", micLevel, micRms, micPeakToPeak,
           live.r, live.g, live.b);
  server.send(200, "application/json", buf);
}

// ── GET /api/state  → full diorama state snapshot ────────────────────────────
// The web app polls this every few seconds to sync changes made on the TFT.
void handleState() {
  // Zone colour hex strings
  char leftHex[8], rightHex[8], centerHex[8];
  snprintf(leftHex,   sizeof(leftHex),   "#%02X%02X%02X", zone[0].r, zone[0].g, zone[0].b);
  snprintf(rightHex,  sizeof(rightHex),  "#%02X%02X%02X", zone[1].r, zone[1].g, zone[1].b);
  snprintf(centerHex, sizeof(centerHex), "#%02X%02X%02X", zone[2].r, zone[2].g, zone[2].b);

  // Current track name — strip leading slash and extension
  char trackName[48] = "";
  if (trackCount > 0) {
    strlcpy(trackName, trackPath[curTrack] + 1, sizeof(trackName));
    char* dot = strrchr(trackName, '.'); if (dot) *dot = 0;
  }

  // Escape lightMode string for JSON (replace " with \")
  char modeEsc[48];
  strlcpy(modeEsc, lightMode.c_str(), sizeof(modeEsc));

  char json[512];
  snprintf(json, sizeof(json),
    "{"
    "\"gateOpen\":%s,"
    "\"lightsOn\":%s,"
    "\"brightness\":%d,"
    "\"lightingMode\":\"%s\","
    "\"soundReactive\":%s,"
    "\"soundIntensity\":%d,"
    "\"fountainOn\":%s,"
    "\"fountainStr\":%d,"
    "\"fountainAux\":%d,"
    "\"volume\":%d,"
    "\"audioPlaying\":%s,"
    "\"audioTrack\":\"%s\","
    "\"fountainColor\":\"%s\","
    "\"fountainAuxColor\":\"%s\","
    "\"circleColor\":\"%s\""
    "}",
    gateOpen       ? "true" : "false",
    lightsOn       ? "true" : "false",
    brightness,
    modeEsc,
    soundReactive  ? "true" : "false",
    soundIntensity,
    fountainOn     ? "true" : "false",
    fountainStr,
    fountainAux,
    volume,
    audioPlaying   ? "true" : "false",
    trackName,
    leftHex,
    rightHex,
    centerHex
  );

  server.send(200, "application/json", json);
}

// ── GET /api/sensors  → live reading for hardware sensors ────────────────────
void handleSensors() {
  // Microphone values are sampled once in loop and shared with the sound API.
  const int micPct = constrain(map(micLevel, 0, 1023, 0, 100), 0, 100);

  // Biometric / Gate state
  const char* gateStateStr = (gateState == GS_OPEN)     ? "OPEN"
                           : (gateState == GS_SCANNING) ? "SCANNING"
                           : (gateState == GS_GOODBYE)  ? "GOODBYE"
                           : "WAITING";

  char luxJson[20];
  if (ambientSensorAvailable) snprintf(luxJson, sizeof(luxJson), "%.2f", ambientLux);
  else strlcpy(luxJson, "null", sizeof(luxJson));

  char buf[512];
  snprintf(buf, sizeof(buf),
    "{"
    "\"ambientLight\":{\"connected\":%s,\"lux\":%s,\"brightness\":%d,\"source\":\"VEML7700 I2C\"},"
    "\"colorSensor\":{\"connected\":%s,\"r\":%u,\"g\":%u,\"b\":%u,\"source\":\"Serial Color Sensor\"},"
    "\"mic\":{\"level\":%d,\"percent\":%d,\"rms\":%d,\"peakToPeak\":%d,\"detected\":%s},"
    "\"biometric\":{\"state\":\"%s\",\"open\":%s,\"authorized\":%s}"
    "}",
    ambientSensorAvailable ? "true" : "false", luxJson, brightness,
    colorSensorAvailable ? "true" : "false",
    detectedColor.r, detectedColor.g, detectedColor.b,
    micLevel, micPct, micRms, micPeakToPeak, micDetected ? "true" : "false",
    gateStateStr,
    gateOpen ? "true" : "false",
    (gateState == GS_OPEN) ? "true" : "false"
  );

  server.send(200, "application/json", buf);
}

// ── GET /api/fingerprint/users  → list all enrolled users ────────────────────
void handleFingerprintUsers() {
  StaticJsonDocument<4096> doc;
  JsonArray usersArray = doc.createNestedArray("users");
  
  for (int i = 0; i < userCount; i++) {
    JsonObject user = usersArray.createNestedObject();
    user["id"] = users[i].id;
    user["name"] = users[i].name;
    user["active"] = users[i].active;
  }
  
  doc["sensorAvailable"] = fpSensorAvailable;
  doc["totalSlots"] = 127;
  doc["usedSlots"] = userCount;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ── GET/POST /api/fingerprint/enroll?name=xxx&step=1|2&id=X  ────────────────
void handleFingerprintEnroll() {
  if (!server.hasArg("name")) {
    server.send(400, "application/json", "{\"error\":\"missing name parameter\"}");
    return;
  }
  
  String name = server.arg("name");
  int step = server.hasArg("step") ? server.arg("step").toInt() : 0;
  int id = server.hasArg("id") ? server.arg("id").toInt() : findNextFreeID();

  if (id <= 0 || id > 127) {
    server.send(400, "application/json", "{\"error\":\"no valid slot ID available\"}");
    return;
  }

  // If sensor not yet available, try to re-probe before giving up
  if (!fpSensorAvailable) {
    Serial.println("[API] Sensor not ready — attempting re-probe...");
    if (!initFingerprintSensor()) {
      server.send(503, "application/json", "{\"success\":false,\"status\":\"error\",\"message\":\"Sensor not found. Check wiring: Sensor TX -> GPIO16, Sensor RX -> GPIO17.\"}");
      return;
    }
  }

  // Real optical biometric sensor enrollment steps
  if (step == 1) {
    // Step 1: First capture
    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"waiting_finger\",\"message\":\"Waiting for finger on sensor...\"}");
      return;
    }
    if (p != FINGERPRINT_OK) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"error\",\"message\":\"Failed to capture image\"}");
      return;
    }
    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"error\",\"message\":\"Failed to convert image\"}");
      return;
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"success\":true,\"status\":\"step1_ok\",\"id\":%d,\"message\":\"First scan OK. Remove and place same finger again.\"}", id);
    server.send(200, "application/json", buf);
    Serial.printf("[API] FP Step 1 captured for ID #%d\n", id);
    return;
  } else if (step == 2) {
    // Step 2: Second capture & model creation
    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"waiting_finger\",\"message\":\"Place same finger on sensor again...\"}");
      return;
    }
    if (p != FINGERPRINT_OK) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"error\",\"message\":\"Failed to capture confirmation image\"}");
      return;
    }
    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"error\",\"message\":\"Failed to convert confirmation image\"}");
      return;
    }
    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"mismatch\",\"message\":\"Prints did not match\"}");
      Serial.println("[API] FP Step 2 prints did not match");
      return;
    }
    p = finger.storeModel(id);
    if (p != FINGERPRINT_OK) {
      server.send(200, "application/json", "{\"success\":false,\"status\":\"error\",\"message\":\"Failed to store model in sensor\"}");
      return;
    }
    addUser(id, name.c_str());
    char buf[180];
    snprintf(buf, sizeof(buf), "{\"success\":true,\"status\":\"enrolled\",\"id\":%d,\"name\":\"%s\",\"message\":\"Fingerprint enrolled successfully!\"}", id, name.c_str());
    server.send(200, "application/json", buf);
    Serial.printf("[API] FP Enrolled user '%s' at ID #%d\n", name.c_str(), id);
    return;
  }

  // step parameter is required (must be 1 or 2)
  server.send(400, "application/json", "{\"success\":false,\"status\":\"error\",\"message\":\"Missing or invalid step parameter. Use step=1 or step=2.\"}");
}

// ── DELETE /api/fingerprint/delete?id=X  → delete fingerprint ────────────────
void handleFingerprintDelete() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json", "{\"error\":\"missing id parameter\"}");
    return;
  }
  
  int id = server.arg("id").toInt();
  
  if (deleteUser(id)) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"success\":true,\"message\":\"User #%d deleted\"}", id);
    server.send(200, "application/json", buf);
    Serial.printf("[API] Deleted user ID #%d\n", id);
  } else {
    server.send(404, "application/json", "{\"error\":\"user not found\"}");
  }
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
  server.on("/api/sensors",          HTTP_OPTIONS, handleOptions);
  server.on("/api/state",            HTTP_OPTIONS, handleOptions);
  server.on("/api/fingerprint/users",   HTTP_OPTIONS, handleOptions);
  server.on("/api/fingerprint/enroll",  HTTP_OPTIONS, handleOptions);
  server.on("/api/fingerprint/delete",  HTTP_OPTIONS, handleOptions);

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
  server.on("/api/sensors",HTTP_GET, []() { addCORSHeaders(); handleSensors();      });
  server.on("/api/state",      HTTP_GET,[]() { addCORSHeaders(); handleState();       });
  
  // Fingerprint API
  server.on("/api/fingerprint/users",  HTTP_GET, []() { addCORSHeaders(); handleFingerprintUsers(); });
  server.on("/api/fingerprint/enroll", HTTP_GET, []() { addCORSHeaders(); handleFingerprintEnroll(); });
  server.on("/api/fingerprint/delete", HTTP_GET, []() { addCORSHeaders(); handleFingerprintDelete(); });

  // ── Static Web App Files (SPIFFS) ──────────────────────────────────────────
  server.serveStatic("/", SPIFFS, "/");

  server.onNotFound([]() {
    addCORSHeaders();
    String uri = server.uri();
    if (uri.startsWith("/api/")) {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }

    // Check if the requested file exists in SPIFFS (plain or gzipped)
    if (SPIFFS.exists(uri) || SPIFFS.exists(uri + ".gz")) {
      String path = SPIFFS.exists(uri + ".gz") ? uri + ".gz" : uri;
      String contentType = "text/plain";
      if (uri.endsWith(".html") || uri == "/") contentType = "text/html";
      else if (uri.endsWith(".css")) contentType = "text/css";
      else if (uri.endsWith(".js")) contentType = "application/javascript";
      else if (uri.endsWith(".png")) contentType = "image/png";
      else if (uri.endsWith(".svg")) contentType = "image/svg+xml";
      else if (uri.endsWith(".ico")) contentType = "image/x-icon";
      else if (uri.endsWith(".json")) contentType = "application/json";
      else if (uri.endsWith(".stl")) contentType = "model/stl";

      File file = SPIFFS.open(path, "r");
      if (file) {
        if (path.endsWith(".gz")) {
          server.sendHeader("Content-Encoding", "gzip");
        }
        server.streamFile(file, contentType);
        file.close();
        return;
      }
    }

    // Fallback to index.html for Single Page Application navigation
    if (SPIFFS.exists("/index.html.gz") || SPIFFS.exists("/index.html")) {
      File file = SPIFFS.open(SPIFFS.exists("/index.html.gz") ? "/index.html.gz" : "/index.html", "r");
      if (file) {
        if (SPIFFS.exists("/index.html.gz")) {
          server.sendHeader("Content-Encoding", "gzip");
        }
        server.streamFile(file, "text/html");
        file.close();
        return;
      }
    }

    // Direct embedded web app fallback (PROGMEM) — runs without SPIFFS upload
    if (serveEmbeddedWebApp(server)) {
      return;
    }

    server.send(404, "text/plain", "404: Not Found");
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// TOUCH CALIBRATION
// ═══════════════════════════════════════════════════════════════════════════════

bool captureTouchCalibrationPoint(uint8_t point, int targetX, int targetY,
                                 int32_t& rawX, int32_t& rawY) {
  const int marker = 12;
  tft.fillScreen(C_BG);
  txt("Touch and hold each marker", W / 2, 28, 2, C_TEXT, C_BG, MC_DATUM);
  txt("Lift your finger after it is recorded", W / 2, 52, 2, C_DIM, C_BG, MC_DATUM);
  tft.drawCircle(targetX, targetY, marker, C_RED);
  tft.drawFastHLine(targetX - marker, targetY, marker * 2 + 1, C_RED);
  tft.drawFastVLine(targetX, targetY - marker, marker * 2 + 1, C_RED);
  Serial.printf("[TOUCH] Calibration point %u: touch target (%d,%d)\n",
                point + 1, targetX, targetY);

  while (tft.getTouchRawZ() <= 350) delay(10);

  int32_t sumX = 0;
  int32_t sumY = 0;
  for (uint8_t sample = 0; sample < 12; sample++) {
    while (tft.getTouchRawZ() <= 350) delay(5);
    uint16_t rawX, rawY;
    tft.getTouchRaw(&rawX, &rawY);
    if (rawX == 0 || rawY == 0 || rawX >= 4095 || rawY >= 4095) {
      Serial.printf("[TOUCH] Invalid calibration sample x=%u y=%u; check TFT SDO/T_DO wiring\n",
                    rawX, rawY);
      while (tft.getTouchRawZ() > 350) delay(10);
      return false;
    }
    sumX += rawX;
    sumY += rawY;
    delay(12);
  }

  rawX = sumX / 12;
  rawY = sumY / 12;
  Serial.printf("[TOUCH] Calibration point %u raw=(%ld,%ld)\n",
                point + 1, (long)rawX, (long)rawY);

  while (tft.getTouchRawZ() > 350) delay(10);
  delay(150);
  return true;
}

bool calibrateTouchRaw() {
  int32_t rawX[4], rawY[4];
  const int inset = 20;
  const int targetX[4] = { inset, inset, W - 1 - inset, W - 1 - inset };
  const int targetY[4] = { inset, H - 1 - inset, inset, H - 1 - inset };

  for (uint8_t point = 0; point < 4; point++) {
    if (!captureTouchCalibrationPoint(point, targetX[point], targetY[point],
                                      rawX[point], rawY[point])) {
      return false;
    }
  }

  const bool rotated =
      abs((long)(rawX[0] - rawX[1])) > abs((long)(rawY[0] - rawY[1]));
  int32_t x0 = rotated ? (rawY[0] + rawY[1]) / 2 : (rawX[0] + rawX[1]) / 2;
  int32_t x1 = rotated ? (rawY[2] + rawY[3]) / 2 : (rawX[2] + rawX[3]) / 2;
  int32_t y0 = rotated ? (rawX[0] + rawX[2]) / 2 : (rawY[0] + rawY[2]) / 2;
  int32_t y1 = rotated ? (rawX[1] + rawX[3]) / 2 : (rawY[1] + rawY[3]) / 2;

  uint16_t flags = rotated ? 1 : 0;
  if (x0 > x1) {
    const int32_t tmp = x0;
    x0 = x1;
    x1 = tmp;
    flags |= 0x02;
  }
  if (y0 > y1) {
    const int32_t tmp = y0;
    y0 = y1;
    y1 = tmp;
    flags |= 0x04;
  }

  const int32_t xSpan = x1 - x0;
  const int32_t ySpan = y1 - y0;
  if (xSpan < 100 || ySpan < 100 || x1 > 4095 || y1 > 4095) {
    Serial.printf("[TOUCH] Calibration failed: spans x=%ld y=%ld\n",
                  (long)xSpan, (long)ySpan);
    return false;
  }

  touchCalData[0] = x0;
  touchCalData[1] = xSpan;
  touchCalData[2] = y0;
  touchCalData[3] = ySpan;
  touchCalData[4] = flags;
  Serial.printf("[TOUCH] Calibration result: %u %u %u %u flags=%u\n",
                touchCalData[0], touchCalData[1], touchCalData[2],
                touchCalData[3], touchCalData[4]);
  return true;
}

void setupTouch() {
  uint8_t  calOK = 0;

  Serial.printf("[TOUCH] TFT_CS=%d TOUCH_CS=%d REPEAT_CAL=%d\n",
                TFT_CS, TOUCH_CS, REPEAT_CAL);

  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] Mount failed");
  }

  if (SPIFFS.exists(CALIBRATION_FILE) && !REPEAT_CAL) {
    File f = SPIFFS.open(CALIBRATION_FILE, "r");
    if (f) {
      if (f.readBytes((char*)touchCalData, sizeof(touchCalData)) == sizeof(touchCalData)) calOK = 1;
      f.close();
    }
  }

  if (calOK &&
      (touchCalData[0] > 4095 || touchCalData[1] < 100 || touchCalData[1] > 4095 ||
       touchCalData[2] > 4095 || touchCalData[3] < 100 || touchCalData[3] > 4095 ||
       touchCalData[4] > 7)) {
    Serial.println("[TOUCH] Saved calibration is invalid; recalibrating");
    calOK = 0;
  }

  if (calOK) {
    tft.setTouch(touchCalData);
    Serial.printf("[TOUCH] Calibration loaded: %u %u %u %u flags=%u\n",
                  touchCalData[0], touchCalData[1], touchCalData[2],
                  touchCalData[3], touchCalData[4]);
  } else {
    Serial.println(REPEAT_CAL
                     ? "[TOUCH] Forced recalibration starting"
                     : "[TOUCH] Calibration missing; starting calibration");
    while (!calibrateTouchRaw()) {
      tft.fillScreen(C_BG);
      txt("Touch calibration failed", W / 2, H / 2 - 12, 2, C_RED, C_BG, MC_DATUM);
      txt("Retrying in 2 seconds", W / 2, H / 2 + 16, 2, C_DIM, C_BG, MC_DATUM);
      delay(2000);
    }

    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (!f) {
      Serial.println("[TOUCH] ERROR: could not open calibration file for writing");
    } else {
      const size_t written = f.write((const unsigned char*)touchCalData, sizeof(touchCalData));
      f.close();
      if (written != sizeof(touchCalData)) {
        Serial.println("[TOUCH] ERROR: calibration file write was incomplete");
      }
    }
    tft.setTouch(touchCalData);
    Serial.printf("[TOUCH] Calibration complete: %u %u %u %u flags=%u\n",
                  touchCalData[0], touchCalData[1], touchCalData[2],
                  touchCalData[3], touchCalData[4]);
  }
  touchCalibrationReady = touchCalData[1] != 0 && touchCalData[3] != 0;
  if (!touchCalibrationReady) Serial.println("[TOUCH] Invalid calibration spans");
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
  setupEncoder();
  Serial.printf("[ENCODER] CLK=%d DT=%d SW=%d\n",
                ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_SW_PIN);
  colorSensorSerial.begin(COLOR_SENSOR_BAUD, SERIAL_8N1,
                          COLOR_SENSOR_RX_PIN, COLOR_SENSOR_TX_PIN);
  Serial.printf("[COLOR UART] Listening RX=%d, ACK TX=%d at %d baud\n",
                COLOR_SENSOR_RX_PIN, COLOR_SENSOR_TX_PIN, COLOR_SENSOR_BAUD);
  setupAmbientSensor();

  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pushZones();

  // ── Display ─────────────────────────────────────────────────────────────
  tft.init();
  tft.setRotation(1);
  setupTouch();
  tft.fillScreen(C_BG);

  // Keep GPIO25-27 free for the rotary encoder until SD/I2S wiring is restored.
  if (SD_AUDIO_ENABLED) {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sdOk = SD.begin(SD_CS, sdSPI, 16000000);
    if (sdOk) {
      scanTracks();
      Serial.printf("[SD] %d track(s) found\n", trackCount);
    } else {
      Serial.println("[SD] Card not found");
    }

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volToAudio(volume));
    xTaskCreatePinnedToCore(audioTask, "audio", 10000, NULL, 2, NULL, 0);
  } else {
    sdOk = false;
    trackCount = 0;
    Serial.println("[SD/AUDIO] Temporarily disabled; GPIO25-27 reserved for rotary encoder");
  }

  // ── Mic ──────────────────────────────────────────────────────────────────
  pinMode(MIC_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(MIC_PIN, ADC_11db);

  // ── Fingerprint sensor ────────────────────────────────────────────────────
  setupFingerprintSensor();
  loadUsers();
  Serial.printf("[FP] User system initialized: %d users\n", userCount);

  // ── WiFi — Access Point ───────────────────────────────────────────────────
  tft.fillScreen(C_BG);
  txt("Starting WiFi AP...", W / 2, H / 2 - 20, 2, C_DIM, C_BG, MC_DATUM);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  delay(500); // give AP time to start

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WiFi] AP started — SSID: %s  IP: %s\n",
                WIFI_AP_SSID, apIP.toString().c_str());

  // Show connection info on TFT
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, 4, C_GREEN);
  txt("WiFi Ready!", W / 2, 30, 4, C_GREEN, C_BG, MC_DATUM);

  txt("Connect your phone/PC to:", W / 2, 80, 2, C_DIM, C_BG, MC_DATUM);
  card(40, 98, 400, 36, C_GREEN, C_GREEN);
  txt(WIFI_AP_SSID, W / 2, 116, 4, C_WHITE, C_GREEN, MC_DATUM);

  txt("Password:", W / 2, 155, 2, C_DIM, C_BG, MC_DATUM);
  txt(WIFI_AP_PASS, W / 2, 173, 2, C_TEXT, C_BG, MC_DATUM);

  txt("Then set ESP32 IP in Settings to:", W / 2, 210, 2, C_DIM, C_BG, MC_DATUM);
  char ipLine[32];
  snprintf(ipLine, sizeof(ipLine), "%s", apIP.toString().c_str());
  card(40, 228, 400, 36, C_AMBER, C_AMBER);
  txt(ipLine, W / 2, 246, 4, C_WHITE, C_AMBER, MC_DATUM);

  delay(4000); // show for 4 seconds then continue to gate screen

  // ── HTTP routes ──────────────────────────────────────────────────────────
  setupRoutes();
  server.begin();
  Serial.println("[HTTP] Server started");

  // ── Gate start ───────────────────────────────────────────────────────────
  enterGateState(GS_WAITING);
}

void loop() {
  updateColorSensor();
  updateAmbientSensor();
  updateMicSensor();
  pollEncoder();

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
  
  // Enrollment polling
  pollEnrollment();
}
