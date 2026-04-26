#include <Servo.h>

// --- Pin Definitions ---
const int enA = 3;
const int in1 = 4;
const int in2 = 5;
const int hopperPin = 6;
const int ir1Pin = 2; 
const int ir2Pin = A0; 
const int servo1Pin = 9;
const int servo2Pin = 10;

Servo servo1;
Servo servo2;

void setup() {
  Serial.begin(9600);
  
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  
  pinMode(hopperPin, OUTPUT);
  digitalWrite(hopperPin, HIGH); // Active Low: HIGH is OFF  
  pinMode(ir1Pin, INPUT);
  
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo1.write(90);
  servo2.write(90);

  Serial.println("=== SYSTEM HARDWARE TEST ===");
  Serial.println("Type '1' to test Hopper Relay");
  Serial.println("Type '2' to test Conveyor");
  Serial.println("Type '3' to test Good Cocoon (End of line)");
  Serial.println("Type '4' to test Defect Sorting (Servo 1)");
  Serial.println("Type '5' to test High Moisture Sorting (Servo 2)");
  Serial.println("Type '6' to read Sensors (Continuous)");
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    if (cmd == '1') {
      Serial.println("Testing Hopper Relay for 2 seconds...");
      digitalWrite(hopperPin, LOW);  // Active Low: LOW is ON
      delay(2000);
      digitalWrite(hopperPin, HIGH); // Active Low: HIGH is OFF
      Serial.println("Hopper test complete.");
    }
    else if (cmd == '2') {
      Serial.println("Testing Conveyor for 3 seconds...");
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      analogWrite(enA, 255);
      delay(3000);
      analogWrite(enA, 0);
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
      Serial.println("Conveyor test complete.");
    }
    else if (cmd == '3') {
      Serial.println("Testing GOOD cocoon flow...");
      Serial.println("Starting Hopper and Conveyor...");
      digitalWrite(hopperPin, LOW);  // Active Low: LOW is ON
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      analogWrite(enA, 255);
      
      delay(2000); // Simulate travel
      Serial.println("Stopping motors...");
      digitalWrite(hopperPin, HIGH); // Active Low: HIGH is OFF
      analogWrite(enA, 0);
    }
    else if (cmd == '4') {
      Serial.println("Testing DEFECT (Camera) sorting...");
      servo1.write(0);
      delay(500);
      servo1.write(90);
      Serial.println("Defect sorting complete.");
    }
    else if (cmd == '5') {
      Serial.println("Testing MOISTURE sorting...");
      servo2.write(180);
      delay(500);
      servo2.write(90);
      Serial.println("Moisture sorting complete.");
    }
    else if (cmd == '6') {
      Serial.println("Reading Sensors for 5 seconds...");
      for(int i=0; i<20; i++) {
        int ir1 = digitalRead(ir1Pin);
        int ir2 = analogRead(ir2Pin);
        int percent = map(ir2, 0, 1023, 0, 100);
        
        Serial.print("IR 1 (Camera): ");
        Serial.print(ir1 == LOW ? "BLOCKED" : "CLEAR");
        Serial.print(" | IR 2 (Moisture Raw): ");
        Serial.print(ir2);
        Serial.print(" (");
        Serial.print(percent);
        Serial.println("%)");
        
        delay(250);
      }
    }
  }
}
