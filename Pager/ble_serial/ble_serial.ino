/*
Project: Meshenger
Module Name: ble_serial.ino
Description:
    Simple BLE serial bridge using NUS service; forwards data from
    the serial console to a connected BLE client.
Inputs:
    - Serial input characters.
Outputs:
    - BLE notifications carrying the input data.
External Sources:
    - BLE.hpp
Author: Team 2
Creation Date: 02/26/2026
*/

// Build as BLE-only pager (no Mesh/Helpers.hpp); BLE.hpp provides stubs for BroadcastPeer/SendTextMessage.
#define PAGER_BLE_STANDALONE

// Custom Libraries
#include "../BLE.hpp"

//utility headers
#include <string>
#include <span>

// setup:
// Initialize BLE with a test suffix for the device name.
void setup() {
  delay(300);  // let boot settle so serial monitor shows clean output
  InitializeBLE("Test");
}

// loop:
// Forward serial input to connected BLE client.
void loop() {
  if (Serial.available() && IsConnected()){
    String input = Serial.readString();
    SendToApp(std::as_bytes(std::span<char>(input)));
  }
  delay(20);
}
