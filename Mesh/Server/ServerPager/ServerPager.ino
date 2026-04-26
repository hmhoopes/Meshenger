/*
Project: Meshenger
Module Name: ServerPager.ino
Description:
    Firmware for a server-side pager ESP32 that bridges between a hardware serial interface
    (connected to a server/backend) and the ESP-NOW mesh network. Receives incoming mesh messages
    via ESP-NOW and forwards them to the server over serial. Receives text messages from the server
    over serial and forwards them to mesh peers.
Inputs:
    - Hardware serial (UART) from server backend containing target MAC (6 bytes) + text message
    - ESP-NOW packets from mesh nodes (Discovery, DiscoveryResponse, Text, ACK, PeerList)
Outputs:
    - Mesh messages via ESP-NOW to other nodes
    - Received mesh messages forwarded to server over hardware serial
    - Discovery announcements broadcast periodically to maintain mesh connectivity
External Sources:
    - Helpers.hpp (mesh initialization, peer management, message dispatch)
    - Message.hpp (Message struct, message types, sending utilities)
    - ServerSerial.hpp (serial I/O with server backend)
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
  if (count >= 100000){
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
