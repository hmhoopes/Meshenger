/*
Project: Meshenger
Module Name: ServerSerial.hpp
Description:
    Serial I/O utilities for bidirectional communication between the server backend and the mesh
    pager (Pager/ble_serial.ino). Provides functions to serialize outgoing Message structs and
    parse incoming text messages with destination MAC addresses from the server.
Inputs:
    - Serialized messages from server backend: "MSG:<target-mac>|<content>" terminated by 0x1E
    - Message structs (for serialization to server)
Outputs:
    - Message serialization to hardware UART (prefixed with "MSG:")
    - Parsed destination MAC and message content as std::pair<String, MAC>
Message Format:
    - Server → Pager: "MSG:<XX:XX:XX:XX:XX:XX><content>\x1E"
      where <XX:XX:XX:XX:XX:XX> is the 6-byte target peer MAC in colon-separated hex format
    - Pager → Server: "MSG:" + Message.to_server_serial()
External Sources:
    - Message.hpp (Message struct and serialization)
    - MAC.hpp (MAC address type and formatting)
Author: Team 2
Creation Date: 04/11/2026
*/

#ifndef SERVER_SERIAL_HPP
#define SERVER_SERIAL_HPP

// Helper functions for communicating with server over serial

// Helper files cont'd
#include "Message.hpp"
#include "MAC.hpp"

// stdlib includes
#include <string>
#include <utility>

void SendToSerial(const Message message) {
  Serial.print("MSG:");
  Serial.print(message.to_server_serial());
}

String ReadTillStop() {
  String ret;
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 0x1E) { // stop symbol
        break;
      }
      ret += c;
    }
  }
  return ret;
}

//input from serial is MSG:target-mac|content, where content is message to be sent and target-mac is the MAC address of the intended recipient
//stop symbol is 0x1E
std::pair<String, MAC> ReadFromSerial() {
  // Read until stop symbol
  String line = ReadTillStop();
  if( line.length() < 4+17 || !line.startsWith("MSG:")) {
    return std::make_pair(String(), MAC()); // return empty message and MAC on invalid input
  }

  // update target mac
  String targetPeerStr = line.substring(4, 4+17); // strip "MSG:" and get target peer string
  MAC targetMac(std::vector<uint8_t>{
    (uint8_t)strtoul(targetPeerStr.substring(0, 2).c_str(), nullptr, 16),
    (uint8_t)strtoul(targetPeerStr.substring(3, 5).c_str(), nullptr, 16),
    (uint8_t)strtoul(targetPeerStr.substring(6, 8).c_str(), nullptr, 16),
    (uint8_t)strtoul(targetPeerStr.substring(9, 11).c_str(), nullptr, 16),
    (uint8_t)strtoul(targetPeerStr.substring(12, 14).c_str(), nullptr, 16),
    (uint8_t)strtoul(targetPeerStr.substring(15, 17).c_str(), nullptr, 16)
  });

  String content = line.substring(4+17); // get message content

  return std::make_pair(content, targetMac);
}

#endif