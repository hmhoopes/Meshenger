/*
Project: Meshenger
Module Name: node.ino
Description:
    Basic node behaviour: announce MAC periodically and allow
    sending text messages to the first discovered peer via serial.
Inputs:
    - Serial input lines.
Outputs:
    - ESP-NOW discovery broadcasts and text messages.
External Sources:
    - Helpers.hpp
Author: Team 2
Creation Date: 02/11/2026
*/

#include "../../Helpers.hpp"

// setup:
// Initialize ESP-NOW for a node device.
void setup() {
  InitializeESPNow();
}

// loop:
// Periodically announce presence and allow sending a text message to the first discovered peer.
void loop() {
  AnnouceMAC();
#ifdef DEBUG
  Serial.println("DBG: Annouced");
#endif
  delay(2000);
  if (Peers.size() != 0){
#ifdef DEBUG
    Serial.println("DBG: Getting front");
#endif
    Peer receiver = Peers.front();
#ifdef DEBUG
    Serial.println("DBG: Waiting for message.");
#endif
    auto inputMessage = Serial.readStringUntil('\n');
    inputMessage.trim(); // Trim whitespace and \r
    SendTextMessage(receiver, inputMessage);
  }
  delay(2000);
}
