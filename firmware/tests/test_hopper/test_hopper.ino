// --- test_hopper.ino ---
// Tests the RC370 DC Motor via the L298N Motor Driver (OUT3 / OUT4)

// Hopper Motor Pins (L298N)
const int enB = 6;  // PWM enable pin for speed control
const int in3 = 7;  // OUT3 direction pin
const int in4 = 8;  // OUT4 direction pin

// Speed: 70% of max (255 * 0.70 ≈ 178)
const int HOPPER_SPEED = 178;

void setup() {
  Serial.begin(9600);
  Serial.println("--- Hopper Motor (L298N) Test ---");

  pinMode(enB, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  // Initialize motor to OFF
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(enB, 0);
}

void loop() {
  Serial.println("Turning Hopper Motor ON (70% speed)...");
  digitalWrite(in3, HIGH);  // OUT3 HIGH
  digitalWrite(in4, LOW);   // OUT4 LOW
  analogWrite(enB, HOPPER_SPEED);
  delay(2000); // Run for 2 seconds

  Serial.println("Turning Hopper Motor OFF...");
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(enB, 0);
  delay(3000); // Rest for 3 seconds
}
