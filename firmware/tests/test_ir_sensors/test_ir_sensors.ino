// --- test_ir_sensors.ino ---
// Tests the Digital IR Sensor and an IR Temperature Sensor (MLX90614)
// Wiring for MLX90614: VIN -> 5V (or 3.3V), GND -> GND, SCL -> A5, SDA -> A4

#include <Wire.h>
#include <Adafruit_MLX90614.h>

const int ir1Pin = 2; // Digital IR

// Initialize the MLX90614 IR Temperature Sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Function to calculate moisture percentage from temperature.
// Based on studies, evaporative cooling causes wet cocoons to have a lower temperature 
// than the ambient environment.
float calculateMoisture(float objectTempC, float ambientTempC) {
  // Calculate temperature difference between ambient and the cocoon
  float tempDiff = ambientTempC - objectTempC;
  
  // If the object is warmer or equal to ambient, assume it's completely dry
  if (tempDiff <= 0) {
    return 0.0; 
  }
  
  // Conversion logic (calibration required for your specific environment):
  // Let's assume a 1.73°C temperature drop corresponds to ~13% moisture.
  // Therefore, Moisture% = tempDiff * 7.5
  float moisturePercent = tempDiff * 7.5;
  
  // Cap the moisture at 100%
  if (moisturePercent > 100.0) {
    moisturePercent = 100.0;
  }
  
  return moisturePercent;
}

void setup() {
  Serial.begin(9600);
  Serial.println("--- IR Sensors Test ---");
  
  pinMode(ir1Pin, INPUT);
  
  // Initialize the IR temperature sensor
  if (!mlx.begin()) {
    Serial.println("Error connecting to MLX sensor. Check wiring (SDA->A4, SCL->A5).");
    while (1); // Halt if sensor not found
  }
}

void loop() {
  // 1. Read Digital IR
  int ir1State = digitalRead(ir1Pin);

  // 2. Read IR Temperature Sensor
  float ambientTempC = mlx.readAmbientTempC();
  float objectTempC = mlx.readObjectTempC();

  // 3. Calculate Moisture using the new function
  float moisturePercent = calculateMoisture(objectTempC, ambientTempC);

  // 4. Print values to Serial Monitor
  Serial.print("IR1 (Digital): ");
  Serial.print(ir1State == LOW ? "BLOCKED" : "CLEAR");
  Serial.print(" \t | \t ");

  Serial.print("Ambient: ");
  Serial.print(ambientTempC);
  Serial.print("C \t Object: ");
  Serial.print(objectTempC);
  Serial.print("C \t | \t ");

  Serial.print("Moisture: ");
  Serial.print(moisturePercent);
  Serial.print("% -> ");

  // 5. Evaluate if wet or dry
  // High moisture if > 13% based on studies
  if (moisturePercent > 13.0) {
    Serial.println("WET / HIGH MOISTURE (>13%)");
  } else {
    Serial.println("DRY");
  }

  delay(500);
}
