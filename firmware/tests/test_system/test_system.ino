#include <Servo.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

// --- Pin Definitions ---
const int enA = 3;
const int in1 = 4;
const int in2 = 5;

// Hopper Motor (L298N)
const int enB = 6;
const int in3 = 7;
const int in4 = 8;
const int HOPPER_SPEED = 178;

const int ir1Pin = 2; 
const int servo1Pin = 9;
const int servo2Pin = 10;

Servo servo1;
Servo servo2;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

float calculateMoisture(float objectTempC, float ambientTempC) {
  float tempDiff = ambientTempC - objectTempC;
  if (tempDiff <= 0) return 0.0; 
  float moisturePercent = tempDiff * 7.5;
  if (moisturePercent > 100.0) moisturePercent = 100.0;
  return moisturePercent;
}

void setup() {
  Serial.begin(9600);
  
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  
  pinMode(enB, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(enB, 0);

  pinMode(ir1Pin, INPUT);
  
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo1.write(90);
  servo2.write(90);

  if (!mlx.begin()) {
    Serial.println("Warning: MLX90614 not detected. Check wiring.");
  }

  Serial.println("=== SYSTEM HARDWARE TEST ===");
  Serial.println("Type '1' to test Hopper Motor");
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
      Serial.println("Testing Hopper Motor for 2 seconds...");
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      analogWrite(enB, HOPPER_SPEED);
      delay(2000);
      analogWrite(enB, 0);
      digitalWrite(in3, LOW);
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
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      analogWrite(enB, HOPPER_SPEED);
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      analogWrite(enA, 255);
      
      delay(2000); // Simulate travel
      Serial.println("Stopping motors...");
      analogWrite(enB, 0);
      digitalWrite(in3, LOW);
      analogWrite(enA, 0);
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
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
        float ambientTempC = mlx.readAmbientTempC();
        float objectTempC = mlx.readObjectTempC();
        float moisturePercent = calculateMoisture(objectTempC, ambientTempC);
        
        Serial.print("IR 1 (Camera): ");
        Serial.print(ir1 == LOW ? "BLOCKED" : "CLEAR");
        Serial.print(" | IR Temp: ");
        Serial.print(objectTempC);
        Serial.print("C | Moisture: ");
        Serial.print(moisturePercent);
        Serial.println("%");
        
        delay(250);
      }
    }
  }
}
