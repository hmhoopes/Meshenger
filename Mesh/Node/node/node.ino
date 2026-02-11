int ledPin = 2; // Default In-Built LED pin for DFRobot Firebeetle ESP32-C3

void setup() {
  pinMode(ledPin, OUTPUT); // Set the LED pin as an output
}

void loop() {
  digitalWrite(ledPin, HIGH); // Turn the LED on
  delay(1000);               // Wait for 1 second
  digitalWrite(ledPin, LOW); // Turn the LED off
  delay(1000);               // Wait for 1 second
}