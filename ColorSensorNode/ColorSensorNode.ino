/*
  Diorama — Color Sensor Node (secondary MCU)

  Runs the color sensor on a SEPARATE microcontroller so the main ESP32
  (Web App + TFT UI) never shares I2C/sensor load with the display stack.

  Wiring to main ESP32 (Diorama.ino UART1):
    This MCU TX  →  ESP32 GPIO 4  (COLOR_RX)
    This MCU RX  ←  ESP32 GPIO 5  (COLOR_TX)
    GND          ↔  GND
    Baud         :  115200

  Protocol (logical RGB, NOT inverted):
    Outgoing (this → main):  RGB:r,g,b\n     e.g. RGB:255,128,64
    Incoming (main → this):  SCAN\n          request one fresh sample

  Also streams a reading every STREAM_MS so /api/sensors stays live.

  Sensor: TCS34725 over I2C (SDA/SCL — default Wire pins for your board).
  Swap readTCS34725() if you use a different sensor; keep the Serial format.
*/

#include <Wire.h>

#define TCS_ADDR    0x29
#define TCS_CMD     0x80
#define TCS_ENABLE  0x00
#define TCS_ATIME   0x01
#define TCS_CONTROL 0x0F
#define TCS_RDATAL  0x16

#define SERIAL_BAUD 115200
#define STREAM_MS   500

struct RGB { uint8_t r, g, b; };

char rxBuf[32];
uint8_t rxLen = 0;
uint32_t lastStreamMs = 0;

bool tcsWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCS_ADDR);
  Wire.write(TCS_CMD | reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readTCS34725(RGB* out) {
  Wire.beginTransmission(TCS_ADDR);
  if (Wire.endTransmission() != 0) return false;

  tcsWrite(TCS_ENABLE,  0x01);  // PON
  delay(3);
  tcsWrite(TCS_ENABLE,  0x03);  // PON + AEN
  tcsWrite(TCS_ATIME,   0xC0);  // ~154 ms
  tcsWrite(TCS_CONTROL, 0x00);  // 1× gain
  delay(160);

  Wire.beginTransmission(TCS_ADDR);
  Wire.write(TCS_CMD | 0xA0 | TCS_RDATAL);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)TCS_ADDR, (uint8_t)8);
  if (Wire.available() < 8) return false;

  uint16_t c = Wire.read() | (Wire.read() << 8);
  uint16_t r = Wire.read() | (Wire.read() << 8);
  uint16_t g = Wire.read() | (Wire.read() << 8);
  uint16_t b = Wire.read() | (Wire.read() << 8);
  if (c == 0) return false;

  out->r = constrain((uint32_t)r * 255 / c, 0, 255);
  out->g = constrain((uint32_t)g * 255 / c, 0, 255);
  out->b = constrain((uint32_t)b * 255 / c, 0, 255);
  return true;
}

void sendRGB(const RGB& c) {
  // Logical colour for the main controller (UI/API). PWM inversion is
  // applied only on the main ESP32 MOSFET outputs — not here.
  Serial.printf("RGB:%d,%d,%d\n", c.r, c.g, c.b);
}

void sampleAndSend() {
  RGB c;
  if (readTCS34725(&c)) {
    sendRGB(c);
  } else {
    Serial.println("RGB:0,0,0");  // still answers SCAN so link can be verified
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin();
  delay(50);
  Serial.println("COLOR_NODE_READY");
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      if (rxLen > 0 && strcasecmp(rxBuf, "SCAN") == 0) {
        sampleAndSend();
        lastStreamMs = millis();
      }
      rxLen = 0;
      continue;
    }
    if (rxLen < sizeof(rxBuf) - 1) rxBuf[rxLen++] = c;
    else rxLen = 0;
  }

  if (millis() - lastStreamMs >= STREAM_MS) {
    lastStreamMs = millis();
    sampleAndSend();
  }
}
