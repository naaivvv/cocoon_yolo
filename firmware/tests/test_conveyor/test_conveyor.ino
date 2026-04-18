// --- test_conveyor.ino ---
// Tests the SGM37-3530 DC12V Gear Motor via the L298N Motor Driver

// Conveyor Motor Pins (L298N)
const int enA = 3;
const int in1 = 4;
const int in2 = 5;

void setup() {
  Serial.begin(115200);
  Serial.println("--- Conveyor Motor Test ---");
  
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);

  // Initialize motor to OFF
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  analogWrite(enA, 0);
}

void loop() {
  Serial.println("Running Conveyor FORWARD for 3 seconds...");
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  analogWrite(enA, 255); // Full speed
  delay(3000);

  Serial.println("Stopping Conveyor for 2 seconds...");
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  analogWrite(enA, 0);
  delay(2000);

  // Note: Most conveyor belts only need to go one way, but we test reverse just in case.
  Serial.println("Running Conveyor REVERSE for 3 seconds...");
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  analogWrite(enA, 255); // Full speed
  delay(3000);

  Serial.println("Stopping Conveyor for 2 seconds...");
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  analogWrite(enA, 0);
  delay(2000);
}
