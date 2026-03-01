/*
 * Pager BLE serial bridge - Nordic UART Service (NUS).
 * Use with the Meshenger web app: connect via Web Bluetooth to send/receive messages.
 *
 * NUS UUIDs (same as web app):
 *   Service:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX (write from client): 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 *   TX (notify to client):  6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 */
// Custom Libraries
#include "../BLE.hpp"

//utility headers
#include <string>
#include <span>

void setup() {
  InitializeBLE();
}

void loop() {
  if (Serial.available() && IsConnected()){
    String input = Serial.readString();
    SendToApp(std::as_bytes(std::span<char>(input)));
  }
  delay(20);
}
