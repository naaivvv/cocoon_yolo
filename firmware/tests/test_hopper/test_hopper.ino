// --- test_hopper.ino ---
// Tests the RC370 DC Motor via the Relay Module

// Hopper Motor Pin (Relay)
const int hopperPin = 6;

void setup() {
  Serial.begin(115200);
  Serial.println("--- Hopper Motor (Relay) Test ---");
  
  pinMode(hopperPin, OUTPUT);
  digitalWrite(hopperPin, LOW); // Start with relay off
}

void loop() {
  Serial.println("Turning Hopper Motor ON...");
  digitalWrite(hopperPin, HIGH); // Assuming HIGH triggers the relay
  delay(2000); // Run for 2 seconds

  Serial.println("Turning Hopper Motor OFF...");
  digitalWrite(hopperPin, LOW); 
  delay(3000); // Rest for 3 seconds
}
