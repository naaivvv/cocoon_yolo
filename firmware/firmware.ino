#include <Servo.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

// --- Pin Definitions ---
// Conveyor Motor (L298N)
const int enA = 3;
const int in1 = 4;
const int in2 = 5;

// Hopper Motor (L298N)
const int enB = 6;
const int in3 = 7;
const int in4 = 8;

const int HOPPER_SPEED = 178; // 70% speed

// Sensors
const int ir1Pin = 2; // Digital IR

// Servos
const int servo1Pin = 9;
const int servo2Pin = 10;

// --- Objects ---
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

// --- State Machine Enums ---
enum SystemState {
  STATE_IDLE,
  STATE_FEEDING,
  STATE_MOVING_TO_CAM,
  STATE_WAITING_CAM_RESULT,
  STATE_SORT_DEFECT,
  STATE_MOVING_TO_MOISTURE,
  STATE_CHECK_MOISTURE,
  STATE_SORT_MOISTURE,
  STATE_MOVE_TO_END
};

SystemState currentState = STATE_IDLE;
bool systemActive = false;

// --- State Variables ---
bool conveyorRunning = false;
bool hopperRunning = false;
int ir1State = HIGH; // Assuming HIGH is clear, LOW is object detected
float currentMoisturePercent = 0.0;

// Non-blocking timers
unsigned long previousTelemetryMillis = 0;
const long telemetryInterval = 500; // 500ms

unsigned long stateTimer = 0; // For states that need delays

// Mock counters for JSON payload
unsigned long totalProcessed = 0;
unsigned long goodProcessed = 0;
unsigned long defectProcessed = 0;
unsigned long moistureProcessed = 0;

void setup() {
  Serial.begin(9600);

  // Conveyor pins
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  stopConveyor();

  // Hopper pins
  pinMode(enB, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  
  // Initialize hopper motor to OFF
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(enB, 0);

  // Sensor pins
  pinMode(ir1Pin, INPUT);

  if (!mlx.begin()) {
    Serial.println("[ERROR] MLX90614 not found. Check wiring (SDA->A4, SCL->A5).");
  }

  // Servos
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  
  // Set default servo positions
  servo1.write(90);
  servo2.write(90);

  Serial.println("[DEBUG] System Initialized");
}

void loop() {
  unsigned long currentMillis = millis();

  readSensors();
  processSerialCommands();
  runStateMachine();

  // Send telemetry every 500ms
  if (currentMillis - previousTelemetryMillis >= telemetryInterval) {
    previousTelemetryMillis = currentMillis;
    sendTelemetryJSON();
  }
}

void readSensors() {
  ir1State = digitalRead(ir1Pin);
  float ambientTempC = mlx.readAmbientTempC();
  float objectTempC = mlx.readObjectTempC();
  currentMoisturePercent = calculateMoisture(objectTempC, ambientTempC);
}

void processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.equalsIgnoreCase("START")) {
      systemActive = true;
      Serial.println("[DEBUG] Command: START");
      if (currentState == STATE_IDLE) {
        changeState(STATE_FEEDING);
      }
    } else if (command.equalsIgnoreCase("STOP")) {
      systemActive = false;
      stopConveyor();
      stopHopper();
      Serial.println("[DEBUG] Command: STOP");
      changeState(STATE_IDLE);
    } else if (command.equalsIgnoreCase("RESET")) {
      totalProcessed = 0;
      goodProcessed = 0;
      defectProcessed = 0;
      moistureProcessed = 0;
      Serial.println("[DEBUG] Command: RESET - Counters zeroed");
    } else if (command.equalsIgnoreCase("DEFECT:YES") && currentState == STATE_WAITING_CAM_RESULT) {
      Serial.println("[DEBUG] Command: DEFECT:YES");
      defectProcessed++;
      changeState(STATE_SORT_DEFECT);
    } else if (command.equalsIgnoreCase("DEFECT:NO") && currentState == STATE_WAITING_CAM_RESULT) {
      Serial.println("[DEBUG] Command: DEFECT:NO");
      changeState(STATE_MOVING_TO_MOISTURE);
    }
  }
}

void changeState(SystemState newState) {
  currentState = newState;
  stateTimer = millis();
  
  // Debug print state transition
  String stateStr = "";
  switch(newState) {
    case STATE_IDLE: stateStr = "IDLE"; break;
    case STATE_FEEDING: stateStr = "FEEDING"; break;
    case STATE_MOVING_TO_CAM: stateStr = "MOVING_TO_CAM"; break;
    case STATE_WAITING_CAM_RESULT: stateStr = "WAITING_CAM_RESULT"; break;
    case STATE_SORT_DEFECT: stateStr = "SORT_DEFECT"; break;
    case STATE_MOVING_TO_MOISTURE: stateStr = "MOVING_TO_MOISTURE"; break;
    case STATE_CHECK_MOISTURE: stateStr = "CHECK_MOISTURE"; break;
    case STATE_SORT_MOISTURE: stateStr = "SORT_MOISTURE"; break;
    case STATE_MOVE_TO_END: stateStr = "MOVE_TO_END"; break;
  }
  Serial.print("[DEBUG] State changed to: ");
  Serial.println(stateStr);
}

void runStateMachine() {
  if (!systemActive) return;

  switch (currentState) {
    case STATE_IDLE:
      // Waiting for START command
      break;

    case STATE_FEEDING:
      // Activate hopper and conveyor together
      startHopper();
      startConveyor();
      totalProcessed++;
      changeState(STATE_MOVING_TO_CAM); // stateTimer resets here
      break;

    case STATE_MOVING_TO_CAM:
      // Guard: wait at least 500ms before checking IR1 to avoid
      // false triggers from vibration or sensor noise at startup.
      if (millis() - stateTimer > 500 && ir1State == LOW) {
        delay(100); // Wait a tiny bit for cocoon to center
        stopHopper();
        stopConveyor(); // Stop conveyor
        changeState(STATE_WAITING_CAM_RESULT);
      }
      break;

    case STATE_WAITING_CAM_RESULT:
      // Waiting for Python to send DEFECT:YES or DEFECT:NO
      break;

    case STATE_SORT_DEFECT:
      // Sweep servo1 slowly to swipe the bad cocoon
      sweepServo(servo1, 90, 0, 10);
      delay(500); 
      sweepServo(servo1, 0, 90, 10);
      
      changeState(STATE_FEEDING); // Next cocoon
      break;

    case STATE_MOVING_TO_MOISTURE:
      startConveyor();
      // Travel time from Camera/IR1 position to Moisture sensor position.
      // Adjust this delay (ms) to match the physical distance on your belt.
      if (millis() - stateTimer > 2000) {
        stopConveyor();
        changeState(STATE_CHECK_MOISTURE);
      }
      break;

    case STATE_CHECK_MOISTURE: {
      // Evaluate moisture
      if (currentMoisturePercent > 13.0) {
        Serial.println("[DEBUG] High moisture detected");
        moistureProcessed++;
        changeState(STATE_SORT_MOISTURE);
      } else {
        Serial.println("[DEBUG] Moisture OK");
        goodProcessed++;
        changeState(STATE_MOVE_TO_END);
      }
      break;
    }

    case STATE_SORT_MOISTURE:
      // Sweep servo2 slowly
      sweepServo(servo2, 90, 180, 10);
      delay(500);
      sweepServo(servo2, 180, 90, 10);
      
      changeState(STATE_FEEDING); // Next cocoon
      break;

    case STATE_MOVE_TO_END:
      startConveyor();
      // Move for 2 seconds to drop good cocoon at the end
      if (millis() - stateTimer > 2000) {
        stopConveyor();
        changeState(STATE_FEEDING); // Next cocoon
      }
      break;
  }
}

void sweepServo(Servo &s, int startPos, int endPos, int stepDelay) {
  if (startPos < endPos) {
    for (int pos = startPos; pos <= endPos; pos++) {
      s.write(pos);
      delay(stepDelay);
    }
  } else {
    for (int pos = startPos; pos >= endPos; pos--) {
      s.write(pos);
      delay(stepDelay);
    }
  }
}

void startConveyor() {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  analogWrite(enA, 255); // Full speed
  conveyorRunning = true;
}

void stopConveyor() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  analogWrite(enA, 0);
  conveyorRunning = false;
}

void startHopper() {
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  analogWrite(enB, HOPPER_SPEED);
  hopperRunning = true;
}

void stopHopper() {
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(enB, 0);
  hopperRunning = false;
}

void sendTelemetryJSON() {
  String motorStatus = conveyorRunning ? "RUNNING" : "STOPPED";
  String ir1Status = (ir1State == LOW) ? "BLOCKED" : "CLEAR";
  String hopperStatus = hopperRunning ? "RUNNING" : "STOPPED";
  
  Serial.print("{\"metrics\":{");
  Serial.print("\"total\":"); Serial.print(totalProcessed); Serial.print(",");
  Serial.print("\"good\":"); Serial.print(goodProcessed); Serial.print(",");
  Serial.print("\"defect\":"); Serial.print(defectProcessed); Serial.print(",");
  Serial.print("\"moisture_reject\":"); Serial.print(moistureProcessed);
  Serial.print("},\"hardware\":{");
  Serial.print("\"motorA\":\""); Serial.print(motorStatus); Serial.print("\",");
  Serial.print("\"hopper\":\""); Serial.print(hopperStatus); Serial.print("\",");
  Serial.print("\"ir1\":\""); Serial.print(ir1Status); Serial.print("\"");
  Serial.print("},\"environment\":{");
  Serial.print("\"moisture\":"); Serial.print(currentMoisturePercent);
  Serial.println("}}");
}
