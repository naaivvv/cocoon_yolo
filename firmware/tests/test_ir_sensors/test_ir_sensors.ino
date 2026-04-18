// --- test_ir_sensors.ino ---
// Tests the Digital and Analog IR Sensors

const int ir1Pin = 2; // Digital IR
const int ir2Pin = A0; // Analog IR (moisture/reflectance)

void setup() {
  Serial.begin(115200);
  Serial.println("--- IR Sensors Test ---");
  
  pinMode(ir1Pin, INPUT);
  // Analog pins don't strictly require pinMode(), but we can do it anyway
  pinMode(ir2Pin, INPUT);
}

void loop() {
  // Read values
  int ir1State = digitalRead(ir1Pin);
  int ir2Value = analogRead(ir2Pin);

  // Print values to Serial Monitor
  Serial.print("IR1 (Digital) State: ");
  Serial.print(ir1State);
  Serial.print(" (");
  if (ir1State == LOW) {
    Serial.print("DETECTED / BLOCKED");
  } else {
    Serial.print("CLEAR");
  }
  Serial.print(") \t | \t ");

  Serial.print("IR2 (Analog) Value: ");
  Serial.println(ir2Value);

  // Delay for readability
  delay(250);
}
