// Helper functions for communicating with server over serial


// Helper files cont'd
#include "Message.hpp"

// stdlib includes
#include <string>
#include <utility>

void SendToSerial(const Message message) {
  Serial.println("------------------------------------------------");
  Serial.print("MSG received:");
  Serial.println(message.to_cstr());
  Serial.println("------------------------------------------------");
}