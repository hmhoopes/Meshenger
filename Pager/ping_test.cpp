/*
Project: Meshenger
Module Name: ping_test.cpp
Description:
    Establishes connection between USB Serial and Bluetooth Serial on ESP32 device.
    This allows communication between a computer and a paired Bluetooth device.
Inputs:
    - Data from USB Serial (computer)
    - Data from Bluetooth Serial (paired device)
Outputs:
    - Forwards USB Serial data to Bluetooth
    - Forwards Bluetooth data to USB Serial
External Sources:
    - esp32 documentation, randomnerdtutorials.com
Author: Team 2
Creation Date: 02/13/2026
*/

#include "BluetoothSerial.h"

String device_name = "ESP32-BT-Slave";

//Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

//Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;
//Runs once at boot. Initializes USB Serial and Bluetooth Serial communication.
void setup() {
  Serial.begin(115200);
  //Bluetooth device name
  SerialBT.begin(device_name);
  //SerialBT.deleteAllBondedDevices(); //Uncomment this to delete paired devices; Must be called after begin
  Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());
}
//Continuously checks for incoming data from either USB Serial or Bluetooth Serial and forwards it.
void loop() {
  //If data is available from USB Serial, send it over Bluetooth
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
  //If data is available from Bluetooth, send it to USB Serial
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  delay(20);
}
