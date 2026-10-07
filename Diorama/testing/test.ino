// Define TB6612FNG Pin Assignments
const int AIN1 = 14;
const int AIN2 = 12;
const int PWMA = 13;  // Must be a PWM capable pin (~ symbol on Arduino)
// const int STBY = 9;  // Connect to 5V if you want to save a pin

void setup() {
  // Set all control pins as outputs
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  // pinMode(STBY, OUTPUT);
  
  // Take the driver out of standby mode
  // digitalWrite(STBY, HIGH);
}

void loop() {
  // 1. Run Pump at Full Speed Forward
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 215); // Speed range: 0 to 255
  delay(3000);

  // // 2. Slow down Pump to Half Speed
  // analogWrite(PWMA, 50); 
  // delay(3000);

  // // 3. Turn Pump Off (Coast to a stop)
  // digitalWrite(AIN1, LOW);
  // digitalWrite(AIN2, LOW);
  // analogWrite(PWMA, 0);
  // delay(5000); 
}
