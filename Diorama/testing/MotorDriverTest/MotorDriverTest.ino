// Standalone ESP32 test for a dual H-bridge driver with IN1-IN4 inputs.
// Motor A uses IN1/IN2; Motor B uses IN3/IN4.
// For an L298N/L293D, enable ENA and ENB (jumpers or HIGH).
// GPIO16/17 are also used by the main diorama sketch's fingerprint UART.

constexpr uint8_t IN1_PIN = 13;
constexpr uint8_t IN2_PIN = 14;
constexpr uint8_t IN3_PIN = 16;
constexpr uint8_t IN4_PIN = 17;
constexpr uint32_t RUN_TIME_MS = 2000;
constexpr uint32_t PAUSE_MS = 1000;

void stopMotors() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
}

void runMotorA(bool forward) {
  digitalWrite(IN1_PIN, forward ? HIGH : LOW);
  digitalWrite(IN2_PIN, forward ? LOW : HIGH);
}

void runMotorB(bool forward) {
  digitalWrite(IN3_PIN, forward ? HIGH : LOW);
  digitalWrite(IN4_PIN, forward ? LOW : HIGH);
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  stopMotors();

  Serial.println("Dual DC motor driver test ready.");
  Serial.println("Motor A: IN1=GPIO13, IN2=GPIO14");
  Serial.println("Motor B: IN3=GPIO16, IN4=GPIO17");
  Serial.println("Each motor runs forward, stops, then reverses.");
  Serial.println("Ensure the driver's enable inputs are enabled.");
}

void loop() {
  Serial.println("Motor A forward");
  runMotorA(true);
  delay(RUN_TIME_MS);
  stopMotors();
  delay(PAUSE_MS);

  Serial.println("Motor A reverse");
  runMotorA(false);
  delay(RUN_TIME_MS);
  stopMotors();
  delay(PAUSE_MS);

  Serial.println("Motor B forward");
  runMotorB(true);
  delay(RUN_TIME_MS);
  stopMotors();
  delay(PAUSE_MS);

  Serial.println("Motor B reverse");
  runMotorB(false);
  delay(RUN_TIME_MS);
  stopMotors();
  delay(PAUSE_MS);
}
