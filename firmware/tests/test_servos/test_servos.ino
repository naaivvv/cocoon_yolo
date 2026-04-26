// --- test_servos.ino ---
// Tests the two SG90 Servo Motors

#include <Servo.h>

const int servo1Pin = 9;
const int servo2Pin = 10;

Servo servo1;
Servo servo2;

void setup() {
  Serial.begin(9600);
  Serial.println("--- Servo Motors Test ---");
  
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);

  // Set to default middle position
  servo1.write(90);
  servo2.write(90);
  delay(1000);
}

void loop() {
  Serial.println("Testing Servo 1 (Defect Gate)...");
  sweepServo(servo1);
  delay(1000);

  Serial.println("Testing Servo 2 (Moisture Gate)...");
  sweepServo(servo2);
  delay(1000);
}

// Function to smoothly sweep a servo back and forth
void sweepServo(Servo &s) {
  Serial.println("Sweeping 90 -> 0");
  for (int pos = 90; pos >= 0; pos--) {
    s.write(pos);
    delay(10);
  }
  delay(500);

  Serial.println("Sweeping 0 -> 180");
  for (int pos = 0; pos <= 180; pos++) {
    s.write(pos);
    delay(10);
  }
  delay(500);

  Serial.println("Sweeping 180 -> 90");
  for (int pos = 180; pos >= 90; pos--) {
    s.write(pos);
    delay(10);
  }
}
