#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

String device_name = "ESP32-BT-Slave";
const int ledPin = 2;

void ledPulse(int ms = 50) {
  digitalWrite(ledPin, HIGH);
  delay(ms);
  digitalWrite(ledPin, LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  if (!SerialBT.begin(device_name)) {
    Serial.println("Failed to start BluetoothSerial");
    while (true) { delay(1000); }
  }

  Serial.printf("Device \"%s\" started. Pair it over Bluetooth (SPP).\n", device_name.c_str());

  //indicate startup
  ledPulse(100);
  delay(100);
  ledPulse(100);
}

void loop() {
  //indicate message send
  while (Serial.available()) {
    uint8_t b = (uint8_t)Serial.read();
    SerialBT.write(b);
  }

  //indicate message receive
  while (SerialBT.available()) {
    uint8_t b = (uint8_t)SerialBT.read();
    Serial.write(b);
    ledPulse(10);
  }

  delay(2);
}