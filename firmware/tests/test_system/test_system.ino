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

bool conveyorRunning = false;
bool hopperRunning = false;
int s1Pos = 90;
int s2Pos = 90;
int ir1State = HIGH;
float currentMoisturePercent = 0.0;

unsigned long previousTelemetryMillis = 0;
const long telemetryInterval = 300; // 300ms (reduced from 500ms)

float calculateMoisture(float objectTempC, float ambientTempC) {
  float tempDiff = ambientTempC - objectTempC;
  if (tempDiff <= 0) return 0.0; 
  float moisturePercent = tempDiff * 7.5;
  if (moisturePercent > 100.0) moisturePercent = 100.0;
  return moisturePercent;
}

void setup() {
  Serial.begin(9600); // Increased baud rate
  
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
  servo1.write(s1Pos);
  servo2.write(s2Pos);

  if (!mlx.begin()) {
    Serial.println("[ERROR] MLX90614 not detected. Check wiring.");
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Read Sensors
  ir1State = digitalRead(ir1Pin);
  float ambientTempC = mlx.readAmbientTempC();
  float objectTempC = mlx.readObjectTempC();
  currentMoisturePercent = calculateMoisture(objectTempC, ambientTempC);

  // Process Serial Commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "HOPPER:1") {
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      analogWrite(enB, HOPPER_SPEED);
      hopperRunning = true;
    } else if (command == "HOPPER:0") {
      digitalWrite(in3, LOW);
      digitalWrite(in4, LOW);
      analogWrite(enB, 0);
      hopperRunning = false;
    } else if (command == "CONV:1") {
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      analogWrite(enA, 255);
      conveyorRunning = true;
    } else if (command == "CONV:0") {
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
      analogWrite(enA, 0);
      conveyorRunning = false;
    } else if (command.startsWith("S1:")) {
      s1Pos = command.substring(3).toInt();
      servo1.write(s1Pos);
    } else if (command.startsWith("S2:")) {
      s2Pos = command.substring(3).toInt();
      servo2.write(s2Pos);
    }
  }

  // Send Telemetry
  if (currentMillis - previousTelemetryMillis >= telemetryInterval) {
    previousTelemetryMillis = currentMillis;
    sendTelemetryJSON();
  }
}

void sendTelemetryJSON() {
  String motorStatus = conveyorRunning ? "RUNNING" : "STOPPED";
  String hopperStatus = hopperRunning ? "RUNNING" : "STOPPED";
  String ir1Status = (ir1State == LOW) ? "BLOCKED" : "CLEAR";
  
  Serial.print("{\"hardware\":{");
  Serial.print("\"motorA\":\""); Serial.print(motorStatus); Serial.print("\",");
  Serial.print("\"hopper\":\""); Serial.print(hopperStatus); Serial.print("\",");
  Serial.print("\"servo1\":"); Serial.print(s1Pos); Serial.print(",");
  Serial.print("\"servo2\":"); Serial.print(s2Pos); Serial.print(",");
  Serial.print("\"ir1\":\""); Serial.print(ir1Status); Serial.print("\"");
  Serial.print("},\"environment\":{");
  Serial.print("\"moisture\":"); Serial.print(currentMoisturePercent);
  Serial.println("}}");
}
