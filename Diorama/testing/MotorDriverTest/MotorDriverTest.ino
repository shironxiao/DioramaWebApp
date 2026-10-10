// Standalone ESP32 test for a 28BYJ-48 stepper with a ULN2003 driver board.
// Use these pins on the slave ESP32; GPIO16/17 are used by the main board's
// fingerprint UART.

constexpr uint8_t IN1_PIN = 13;
constexpr uint8_t IN2_PIN = 14;
constexpr uint8_t IN3_PIN = 16;
constexpr uint8_t IN4_PIN = 17;
constexpr uint32_t RUN_TIME_MS = 2000;
constexpr uint32_t PAUSE_MS = 1000;
constexpr uint32_t STEP_INTERVAL_MS = 2;
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

uint8_t stepIndex = 0;

void setPhase(uint8_t phase) {
  digitalWrite(IN1_PIN, MOTOR_PHASES[phase][0] ? HIGH : LOW);
  digitalWrite(IN2_PIN, MOTOR_PHASES[phase][1] ? HIGH : LOW);
  digitalWrite(IN3_PIN, MOTOR_PHASES[phase][2] ? HIGH : LOW);
  digitalWrite(IN4_PIN, MOTOR_PHASES[phase][3] ? HIGH : LOW);
}

void stopMotor() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
}

void runMotor(bool forward, uint32_t durationMs) {
  const uint32_t startedAt = millis();
  uint32_t lastStepAt = startedAt;

  while (millis() - startedAt < durationMs) {
    const uint32_t now = millis();
    if (now - lastStepAt >= STEP_INTERVAL_MS) {
      stepIndex = forward ? (stepIndex + 1) % 8 : (stepIndex + 7) % 8;
      setPhase(stepIndex);
      lastStepAt = now;
    }
    delay(1);
  }
  stopMotor();
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  stopMotor();

  Serial.println("28BYJ-48 + ULN2003 stepper test ready.");
  Serial.println("Inputs: IN1=GPIO13, IN2=GPIO14, IN3=GPIO16, IN4=GPIO17");
}

void loop() {
  Serial.println("Stepper forward");
  runMotor(true, RUN_TIME_MS);
  delay(PAUSE_MS);

  Serial.println("Stepper reverse");
  runMotor(false, RUN_TIME_MS);
  delay(PAUSE_MS);
}
