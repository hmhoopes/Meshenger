/*
Project: Meshenger
Module Name: connection_blink.ino
Description:
    Implements LED indicators for message received, message sent,
    and Bluetooth connection status using an ESP32.
Inputs:
    - Serial input from USB (computer or another microcontroller)
Outputs:
    - Sends and receives data over Bluetooth Serial
    - LED blinks to indicate activity (startup and message receive)
External Sources:
    - esp_now documentation, Medium
Author: Team 2
Creation Date: 02/19/2026
*/

#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

String device_name = "ESP32-BT-Slave";
const int ledPin = 2;
//Briefly turns the LED on and off to indicate an event.
void ledPulse(int ms = 50) {
  digitalWrite(ledPin, HIGH);
  delay(ms);
  digitalWrite(ledPin, LOW);
}

//Runs once at boot. Initializes serial communication, Bluetooth, and LED pin.
void setup() {
  Serial.begin(115200);
  //Configure LED pin as output and ensure it starts off
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  //Start Bluetooth Serial
  if (!SerialBT.begin(device_name)) {
    //If Bluetooth fails to initialize, print error and pause
    Serial.println("Failed to start BluetoothSerial");
    while (true) { 
      delay(1000); 
    }
  }

  //Print ready for connection message to Serial Monitor
  Serial.printf("Device \"%s\" started. Pair it over Bluetooth (SPP).\n", device_name.c_str());

  //Indicate connection established: blink LED twice
  ledPulse(100);
  delay(100);
  ledPulse(100);
}
//Handles communication between USB Serial and Bluetooth Serial.
void loop() {
  //Forward data from USB Serial to Bluetooth (message send)
  while (Serial.available()) {
    uint8_t b = (uint8_t)Serial.read();
    SerialBT.write(b);
  }

  //Forward data from Bluetooth to USB Serial (message receive)
  while (SerialBT.available()) {
    uint8_t b = (uint8_t)SerialBT.read();
    Serial.write(b);
    ledPulse(10);
  }

  delay(2);
}
