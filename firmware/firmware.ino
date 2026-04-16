#include <Servo.h>

// --- Pin Definitions ---
// Motor (L298N)
const int enA = 3;
const int in1 = 4;
const int in2 = 5;

// Sensors
const int ir1Pin = 2; // Digital IR
const int ir2Pin = A0; // Analog IR (moisture/reflectance)

// Servos
const int servo1Pin = 9;
const int servo2Pin = 10;

// Pump
const int pumpPin = 6;

// --- Objects ---
Servo servo1;
Servo servo2;

// --- State Variables ---
bool motorRunning = false;
bool pumpRunning = false;
int ir1State = HIGH; // Assuming HIGH is clear, LOW is object detected
int ir2Value = 0;

// Non-blocking timers
unsigned long previousTelemetryMillis = 0;
const long telemetryInterval = 500; // 500ms

// Mock counters for JSON payload
unsigned long totalProcessed = 0;
unsigned long goodProcessed = 0;
unsigned long badProcessed = 0;

void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  
  // Default motor off
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  analogWrite(enA, 0);

  // Sensor pins
  pinMode(ir1Pin, INPUT);
  // Analog pin A0 doesn't need explicit pinMode in Arduino

  // Servos
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  
  // Set default servo positions
  servo1.write(90);
  servo2.write(90);

  // Pump pin
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);
}

void loop() {
  unsigned long currentMillis = millis();

  readSensors();
  handleMotorLogic();
  processSerialCommands();

  // Send telemetry every 500ms
  if (currentMillis - previousTelemetryMillis >= telemetryInterval) {
    previousTelemetryMillis = currentMillis;
    sendTelemetryJSON();
  }
}

void readSensors() {
  ir1State = digitalRead(ir1Pin);
  ir2Value = analogRead(ir2Pin);
}

void handleMotorLogic() {
  // If IR1 is triggered (assuming LOW = triggered), stop motor immediately
  if (ir1State == LOW && motorRunning) {
    stopMotor();
    motorRunning = false;
    // For demo purposes, we count an item when blocked
    totalProcessed++;
  }
}

void processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.equalsIgnoreCase("START")) {
      startMotor();
      motorRunning = true;
    } else if (command.equalsIgnoreCase("STOP")) {
      stopMotor();
      motorRunning = false;
    } else if (command.equalsIgnoreCase("PUMP_ON")) {
      digitalWrite(pumpPin, HIGH);
      pumpRunning = true;
    } else if (command.equalsIgnoreCase("PUMP_OFF")) {
      digitalWrite(pumpPin, LOW);
      pumpRunning = false;
    } else if (command.startsWith("SERVO1:")) {
      int pos = command.substring(7).toInt();
      if(pos >= 0 && pos <= 180) servo1.write(pos);
    } else if (command.startsWith("SERVO2:")) {
      int pos = command.substring(7).toInt();
      if(pos >= 0 && pos <= 180) servo2.write(pos);
    } else if (command.startsWith("ADD_GOOD")) {
       goodProcessed++;
    } else if (command.startsWith("ADD_BAD")) {
       badProcessed++;
    }
  }
}

void startMotor() {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  analogWrite(enA, 255); // Full speed
}

void stopMotor() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  analogWrite(enA, 0);
}

void sendTelemetryJSON() {
  // Determine states for UI
  String motorStatus = motorRunning ? "RUNNING" : "STOPPED";
  String ir1Status = (ir1State == LOW) ? "BLOCKED" : "CLEAR";
  
  // We can format the analog value nicely. 
  // Maybe map it or just send raw. Let's send raw under environment.
  
  // Construct non-blocking JSON
  Serial.print("{\"metrics\":{");
  Serial.print("\"total\":"); Serial.print(totalProcessed); Serial.print(",");
  Serial.print("\"good\":"); Serial.print(goodProcessed); Serial.print(",");
  Serial.print("\"bad\":"); Serial.print(badProcessed);
  Serial.print("},\"hardware\":{");
  Serial.print("\"motorA\":\""); Serial.print(motorStatus); Serial.print("\",");
  Serial.print("\"motorB\":\"STOPPED\","); // Assuming mock second motor if needed
  Serial.print("\"ir1\":\""); Serial.print(ir1Status); Serial.print("\",");
  Serial.print("\"ir2\":\"CLEAR\""); // Placeholder logic for IR2 status if needed
  Serial.print("},\"environment\":{");
  Serial.print("\"moisture\":"); Serial.print(ir2Value); Serial.print(",");
  Serial.print("\"temp\":24.5"); // Fixed default or placeholder
  Serial.print(",\"humidity\":60.0"); // Fixed default or placeholder
  Serial.println("}}");
}
