#include <SoftwareSerial.h>

// Nano D3 -> level shifter -> ESP32 GPIO36 (test RGB data).
// ESP32 GPIO22 -> level shifter -> Nano D2 (ACK return).
// Connect both boards' GND. Generated colors test UART only, not the sensor.
constexpr uint8_t NANO_RX_PIN = 2;
constexpr uint8_t NANO_TX_PIN = 3;
constexpr unsigned long DEBUG_BAUD = 115200;
constexpr unsigned long ESP32_BAUD = 9600;
constexpr unsigned long ACK_TIMEOUT_MS = 2500;

SoftwareSerial esp32Link(NANO_RX_PIN, NANO_TX_PIN);

static const uint8_t colors[][3] = {
  {255, 0, 0},
  {0, 255, 0},
  {0, 0, 255},
  {255, 255, 255}
};
constexpr size_t COLOR_COUNT = sizeof(colors) / sizeof(colors[0]);

uint8_t colorIndex = 0;
uint8_t sentRed = 0;
uint8_t sentGreen = 0;
uint8_t sentBlue = 0;
uint32_t lastSendMs = 0;
uint32_t ackDeadlineMs = 0;
bool waitingForAck = false;
char ackLine[48];
size_t ackLength = 0;

void sendTestColor() {
  const uint8_t* color = colors[colorIndex];
  sentRed = color[0];
  sentGreen = color[1];
  sentBlue = color[2];

  esp32Link.print(F("{\"r\":"));
  esp32Link.print(sentRed);
  esp32Link.print(F(",\"g\":"));
  esp32Link.print(sentGreen);
  esp32Link.print(F(",\"b\":"));
  esp32Link.print(sentBlue);
  esp32Link.println(F("}"));

  Serial.print(F("[TX SENT] RGB="));
  Serial.print(sentRed);
  Serial.print(',');
  Serial.print(sentGreen);
  Serial.print(',');
  Serial.println(sentBlue);

  waitingForAck = true;
  ackDeadlineMs = millis() + ACK_TIMEOUT_MS;
  colorIndex = (colorIndex + 1) % COLOR_COUNT;
  lastSendMs = millis();
}

void processAckLine() {
  ackLine[ackLength] = '\0';
  char expected[32];
  snprintf(expected, sizeof(expected), "ACK,%u,%u,%u",
           sentRed, sentGreen, sentBlue);

  if (strcmp(ackLine, expected) == 0) {
    waitingForAck = false;
    Serial.print(F("[LINK OK] ESP32 received RGB="));
    Serial.print(sentRed);
    Serial.print(',');
    Serial.print(sentGreen);
    Serial.print(',');
    Serial.println(sentBlue);
  } else {
    Serial.print(F("[ACK MISMATCH] Received: "));
    Serial.println(ackLine);
  }
  ackLength = 0;
}

void readEsp32Acks() {
  while (esp32Link.available()) {
    const char value = (char)esp32Link.read();
    if (value == '\n' || value == '\r') {
      if (ackLength > 0) processAckLine();
    } else if (ackLength < sizeof(ackLine) - 1) {
      ackLine[ackLength++] = value;
    } else {
      ackLength = 0;
      Serial.println(F("[ACK ERROR] ESP32 response too long"));
    }
  }
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  esp32Link.begin(ESP32_BAUD);
  Serial.println(F("UART test: Nano D3 TX -> ESP32 GPIO36 RX."));
  Serial.println(F("ACK return: ESP32 GPIO22 TX -> Nano D2 RX."));
  Serial.println(F("Expected: [LINK OK] for each generated RGB packet."));
  Serial.println(F("This test does not read the physical color sensor."));
}

void loop() {
  readEsp32Acks();

  const uint32_t now = millis();
  if (waitingForAck && (int32_t)(now - ackDeadlineMs) >= 0) {
    waitingForAck = false;
    Serial.println(F("[NO ACK] Nano sent data, but no matching ESP32 reply arrived."));
    Serial.println(F("Check GPIO36/GPIO22 paths, level shifter directions, common GND, and ESP32 firmware."));
  }

  if (!waitingForAck && now - lastSendMs >= 1000) {
    sendTestColor();
  }
}
