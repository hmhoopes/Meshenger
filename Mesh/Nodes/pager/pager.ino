/*
Project: Meshenger
Module Name: pager.ino
Description:
    Entry point for pager firmware; sets pager mode and initializes
    ESP-NOW and BLE. Contains simple debug loop.
Inputs:
    - None (uses global configuration).
Outputs:
    - Starts mesh discovery and BLE advertising.
External Sources:
    - Helpers.hpp, BLE.hpp
Author: Team 2
Creation Date: 02/28/2026
*/

// Custom library headers
#include "../../BLE.hpp"
#include "../../Helpers.hpp"

#include <memory>


// setup:
// Configure this device as a pager, initialize ESP-NOW and BLE.
void setup() {
  SetPagerMode();
  InitializeESPNow(); 
  InitializeBLE(GetMACAddress().to_arduinostr());
}

// loop:
// Main loop for pager (currently only runs debug BLE send when enabled).
void loop() {
#ifdef SENDTOAPP_DEBUG
  std::string message = "hellooooooooooo ";
  if (IsConnected()){
    SendToApp(
      std::as_bytes(
        std::span<char>(reinterpret_cast<char *>(message.data()), message.size())
      )
    );
  }
#endif
  AnnouceMAC();

/* Used to determine message length for App/script.js
  Serial.print("message size: ");
  Serial.println(sizeof(Message));
  Serial.print("mesage max length: ");
  Serial.println(MessageSize); */
  delay(10000);
}
