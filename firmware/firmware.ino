#include <Servo.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

// --- Pin Definitions ---
// Conveyor Motor (L298N)
const int enA = 3;
const int in1 = 4;
const int in2 = 5;

int conveyorSpeed = 200;

// Hopper Motor (L298N)
const int enB = 6;
const int in3 = 7;
const int in4 = 8;

const int HOPPER_SPEED = 255; // 100% speed

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
  STATE_SORT_MOISTURE,
  STATE_MOVE_TO_END
};

// Forward declaration to fix Arduino IDE compilation error
void changeState(SystemState newState);

volatile SystemState currentState = STATE_IDLE;
bool systemActive = false;

// --- State Variables ---
volatile bool conveyorRunning = false;
bool hopperRunning = false;
int ir1State = HIGH; // Assuming HIGH is clear, LOW is object detected
float currentMoisturePercent = 0.0;

// Hardware interrupt flag for instant IR detection
volatile bool irInterruptFlag = false;

// Non-blocking timers
unsigned long previousTelemetryMillis = 0;
const long telemetryInterval = 500; // 500ms is plenty for dashboard updates

unsigned long previousSensorMillis = 0;
const long sensorInterval = 500; // MLX90614 temperature read interval

unsigned long stateTimer = 0; // For states that need delays
unsigned long irLowStartTime = 0; // For IR debounce

// Mock counters for JSON payload
unsigned long totalProcessed = 0;
unsigned long goodProcessed = 0;
unsigned long defectProcessed = 0;
unsigned long moistureProcessed = 0;

// ISR: fires on FALLING edge of IR sensor (object detected)
// Stops motor pins directly from interrupt — works even during I2C blocking
void irFallingISR() {
  if (conveyorRunning && currentState == STATE_MOVING_TO_CAM) {
    // Instant motor stop — digitalWrite is safe in AVR ISR
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(enA, 0);
    conveyorRunning = false;
  }
  irInterruptFlag = true;
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10); // Prevent readStringUntil blocking for 1000ms default

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

  // Attach hardware interrupt on pin 2 (INT0) for instant IR detection
  attachInterrupt(digitalPinToInterrupt(ir1Pin), irFallingISR, FALLING);

  if (!mlx.begin()) {
    Serial.println("[ERROR] MLX90614 not found. Check wiring (SDA->A4, SCL->A5).");
  }

  // Servos
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  
  // Set default servo positions
  servo1.write(180);
  servo2.write(90);

  Serial.println("[DEBUG] System Initialized");
}

void loop() {
  unsigned long currentMillis = millis();

  // FAST PATH: IR sensor read every loop (~4 microseconds)
  ir1State = digitalRead(ir1Pin);

  processSerialCommands();
  runStateMachine();

  // SLOW PATH: Temperature/moisture only every 500ms (I2C takes ~60-100ms)
  if (currentMillis - previousSensorMillis >= sensorInterval) {
    previousSensorMillis = currentMillis;
    readTemperatureSensor();
  }

  // Send telemetry at a reasonable rate
  if (currentMillis - previousTelemetryMillis >= telemetryInterval) {
    previousTelemetryMillis = currentMillis;
    sendTelemetryJSON();
  }
}

void readTemperatureSensor() {
  // MLX90614 I2C reads (~30-50ms each) — only called every 500ms
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
      changeState(STATE_SORT_DEFECT);
    } else if (command.equalsIgnoreCase("DEFECT:NO") && currentState == STATE_WAITING_CAM_RESULT) {
      Serial.println("[DEBUG] Command: DEFECT:NO");
      if (currentMoisturePercent > 13.0) {
        Serial.println("[DEBUG] High moisture detected");
        changeState(STATE_SORT_MOISTURE);
      } else {
        Serial.println("[DEBUG] Moisture OK");
        changeState(STATE_MOVE_TO_END);
      }
    } else if (command.startsWith("SPEED:")) {
      int speedVal = command.substring(6).toInt();
      if (speedVal >= 0 && speedVal <= 255) {
        conveyorSpeed = speedVal;
        Serial.print("[DEBUG] Conveyor speed set to ");
        Serial.println(conveyorSpeed);
        if (conveyorRunning) analogWrite(enA, conveyorSpeed);
      }
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
      changeState(STATE_MOVING_TO_CAM); // stateTimer resets here
      break;

    case STATE_MOVING_TO_CAM:
      // Hopper runs for exactly 2000ms
      if (hopperRunning && millis() - stateTimer > 2000) {
        stopHopper();
      }

      // Guard: wait at least 250ms before acting on IR to let previous cocoon clear
      if (millis() - stateTimer > 250) {
        // CHECK 1: Hardware interrupt already fired (instant, even during I2C)
        if (irInterruptFlag) {
          // Motor was already stopped by ISR — just finalize
          stopConveyor(); // Ensure full stop (ISR may not have cleared PWM fully)
          if (hopperRunning) stopHopper();
          Serial.println("TRIG");
          irInterruptFlag = false;
          irLowStartTime = 0;
          changeState(STATE_WAITING_CAM_RESULT);
        }
        // CHECK 2: Polling fallback (for cases where interrupt was missed)
        else if (ir1State == LOW) {
          if (irLowStartTime == 0) {
            irLowStartTime = millis();
          } else if (millis() - irLowStartTime >= 30) {
            stopConveyor();
            if (hopperRunning) stopHopper();
            Serial.println("TRIG");
            irLowStartTime = 0;
            changeState(STATE_WAITING_CAM_RESULT);
          }
        } else {
          irLowStartTime = 0;
        }
      } else {
        // During guard period, clear any spurious interrupt flags
        irInterruptFlag = false;
        irLowStartTime = 0;
      }
      break;

    case STATE_WAITING_CAM_RESULT:
      // Waiting for Python to send DEFECT:YES or DEFECT:NO
      break;

    case STATE_SORT_DEFECT:
      // Sweep servo1 gently to 180 and back to push the bad cocoon
      sweepServo(servo1, 180, 0, 15);
      delay(500); 
      sweepServo(servo1, 0, 180, 10);
      
      defectProcessed++;
      totalProcessed++;
      changeState(STATE_FEEDING); // Next cocoon
      break;

    case STATE_SORT_MOISTURE:
      if (!conveyorRunning) {
        sweepServo(servo2, 90, 160, 10); // Set blocking angle gently
        startConveyor();   // Run conveyor to move cocoon to the block
      }
      
      if (millis() - stateTimer > 4000) { // Give it 4 seconds to slide off
        stopConveyor();
        sweepServo(servo2, 160, 90, 10); // Reset servo back to neutral gently
        moistureProcessed++;
        totalProcessed++;
        changeState(STATE_FEEDING); // Next cocoon
      }
      break;

    case STATE_MOVE_TO_END:
      startConveyor();
      // Move for 2 seconds to drop good cocoon at the end
      if (millis() - stateTimer > 2000) {
        stopConveyor();
        goodProcessed++;
        totalProcessed++;
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
  analogWrite(enA, conveyorSpeed);
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
  // Single-buffer approach: build entire JSON in one shot to reduce serial overhead
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"metrics\":{\"total\":%lu,\"good\":%lu,\"defect\":%lu,\"moisture_reject\":%lu},"
    "\"hardware\":{\"motorA\":\"%s\",\"hopper\":\"%s\",\"ir1\":\"%s\"},"
    "\"environment\":{\"moisture\":%d}}",
    totalProcessed, goodProcessed, defectProcessed, moistureProcessed,
    conveyorRunning ? "RUNNING" : "STOPPED",
    hopperRunning ? "RUNNING" : "STOPPED",
    (ir1State == LOW) ? "BLOCKED" : "CLEAR",
    (int)currentMoisturePercent);
  Serial.println(buf);
}
