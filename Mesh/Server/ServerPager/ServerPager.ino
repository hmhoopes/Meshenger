/*
Project: Meshenger
Module Name: ServerPager.ino
Description:
Inputs:
    - 
Outputs:
    - 
External Sources:
    - Helpers.hpp
Author: Team 2
Creation Date: 04/11/2026
*/

// Custom library headers
#include "../../Helpers.hpp"
#include "../../Message.hpp"
#include "../../ServerSerial.hpp"

#include <memory>
#include <utility>

// setup:
// Configure this device as a pager, initialize ESP-NOW and BLE.
void setup() {
  String deviceUsername = "server-pager";
  InitializeESPNow();
}

// loop:
// Main loop for pager
void loop() {
  static int count = 0;
  if (count >= 10000){
    count = 0;
    AnnouceMAC();
  }
#ifdef SERIAL_LOG_DEBUG
  Serial.println("----------------testmessage----------------");
#endif
  auto [readyMessage, targetMac] = ReadFromSerial();
  if (targetMac != BroadcastMAC) {
    SendTextMessage(targetMac, readyMessage);
  }
  count++;
  delay(1);
}
