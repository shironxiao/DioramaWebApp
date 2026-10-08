#include <WiFi.h>

const char* WIFI_SSID = "Diorama-Park";
const char* WIFI_PASSWORD = "diorama123";

const unsigned long RECONNECT_INTERVAL_MS = 10000;
const unsigned long STATUS_INTERVAL_MS = 10000;

unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusPrint = 0;
bool wasConnected = false;

void printConnectionStatus() {
  Serial.println("Connected to main ESP32 access point.");
  Serial.print("Network: ");
  Serial.println(WiFi.SSID());
  Serial.print("Slave IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Main ESP32 AP IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Signal strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Slave ESP32 Wi-Fi connection check");
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastReconnectAttempt = millis();
}

void loop() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const unsigned long now = millis();

  if (connected) {
    if (!wasConnected) {
      printConnectionStatus();
      lastStatusPrint = now;
    } else if (now - lastStatusPrint >= STATUS_INTERVAL_MS) {
      Serial.print("Still connected. Slave IP: ");
      Serial.println(WiFi.localIP());
      lastStatusPrint = now;
    }
  } else {
    if (wasConnected) {
      Serial.println("Wi-Fi connection lost.");
    }

    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
      Serial.println("Not connected; trying to reconnect...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastReconnectAttempt = now;
    }
  }

  wasConnected = connected;
  delay(100);
}
