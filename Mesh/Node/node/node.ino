#include "../../Helpers.hpp"

void setup() {
  InitializeESPNow();
  RegisterListen();
}

void loop() {
  AnnouceMAC();
  delay(2000);
  Peer receiver = Peers.front();
  Message message;
  strncpy(message.info, "Hello", MessageSize - 1);  // Prevents buffer overflow
  message.info[MessageSize - 1] = '\0';  // Ensure null termination
  message.type = MessageType::Text;
  message.id = 1;
  bool success = SendMessage(receiver, message);
  if (!success) {
    Serial.println("Could not send text message.");
  }
  delay(2000);
}
