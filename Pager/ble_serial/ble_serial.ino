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

// Custom Libraries
#include "../BLE.hpp"

//utility headers
#include <string>
#include <span>

// setup:
// Initialize mesh stack in pager mode, then bring up BLE.
void setup() {
  delay(300);  // let boot settle so serial monitor shows clean output
  SetPagerMode();       // sets isPager = true so HandleText forwards to BLE
  InitializeESPNow();   // initializes WiFi STA, ESP-NOW, broadcast peer, and rx callback

  // Append last 2 bytes of MAC as a unique 4-char hex suffix, e.g. "Meshenger-Pager-A403"
  char suffix[7];
  uint8_t* macBytes = SelfMAC.GetAddressArray();
  snprintf(suffix, sizeof(suffix), "-%02X%02X", macBytes[4], macBytes[5]);
  InitializeBLE(String(suffix));
}

// loop:
// Periodically announce presence on the mesh so nodes can discover the pager.
void loop() {
  AnnouceMAC();
  delay(2000);
}
