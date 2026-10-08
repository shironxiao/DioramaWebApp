#include <WiFi.h>
#include <WebServer.h>

const char* WIFI_SSID = "Diorama-Park";
const char* WIFI_PASSWORD = "diorama123";

const IPAddress SLAVE_IP(192, 168, 4, 200);
const IPAddress GATEWAY_IP(192, 168, 4, 1);
const IPAddress SUBNET_MASK(255, 255, 255, 0);

const unsigned long RECONNECT_INTERVAL_MS = 10000;
const unsigned long STATUS_INTERVAL_MS = 10000;

WebServer server(80);
unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusPrint = 0;
bool wasConnected = false;
bool ledIsOn = false;

void handleStatus() {
  String response = "{\"connected\":true,\"ip\":\"";
  response += WiFi.localIP().toString();
  response += "\"}";
  server.send(200, "application/json", response);
}

void handleCommand() {
  if (!server.hasArg("value")) {
    server.send(400, "application/json", "{\"error\":\"missing value parameter\"}");
    Serial.println("[COMMAND] Rejected: missing value parameter");
    return;
  }

  const String command = server.arg("value");
  if (command == "LED_ON") {
    ledIsOn = true;
  } else if (command == "LED_OFF") {
    ledIsOn = false;
  } else if (command != "PING") {
    server.send(400, "application/json", "{\"error\":\"unknown command; use LED_ON, LED_OFF, or PING\"}");
    Serial.printf("[COMMAND] Rejected unknown command: %s\n", command.c_str());
    return;
  }

  Serial.printf("[COMMAND] Received %s\n", command.c_str());
  String response = "{\"success\":true,\"command\":\"";
  response += command;
  response += "\",\"ledOn\":";
  response += ledIsOn ? "true}" : "false}";
  server.send(200, "application/json", response);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("NodeMCU-32S slave ESP32 starting");
  Serial.print("Connecting to main ESP32 network: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (!WiFi.config(SLAVE_IP, GATEWAY_IP, SUBNET_MASK)) {
    Serial.println("[WiFi] Failed to configure static IP 192.168.4.200");
  }

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "Slave ESP32 is ready");
  });
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/command", HTTP_GET, handleCommand);
  server.begin();
  Serial.println("[HTTP] Slave command server started on port 80");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastReconnectAttempt = millis();
}

void loop() {
  server.handleClient();

  const bool connected = WiFi.status() == WL_CONNECTED;
  const unsigned long now = millis();

  if (connected) {
    if (!wasConnected) {
      Serial.println("[WiFi] Connected to main ESP32 access point");
      Serial.print("[WiFi] Slave IP: ");
      Serial.println(WiFi.localIP());
      Serial.println("[COMMAND] Main ESP32 can now send commands");
      Serial.print("[WiFi] Main ESP32 IP: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("[WiFi] Signal strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      lastStatusPrint = now;
    } else if (now - lastStatusPrint >= STATUS_INTERVAL_MS) {
      Serial.print("[WiFi] Still connected; IP: ");
      Serial.println(WiFi.localIP());
      lastStatusPrint = now;
    }
  } else {
    if (wasConnected) {
      Serial.println("[WiFi] Connection lost");
    }

    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
      Serial.println("[WiFi] Not connected; trying to reconnect...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastReconnectAttempt = now;
    }
  }

  wasConnected = connected;
  delay(10);
}
