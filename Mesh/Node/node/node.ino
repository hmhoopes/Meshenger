#include "../../Helpers.hpp"

//0 for receiver, 1 for sender
#define Sender 1

void setup() {
  InitializeESPNow();
#if Sender
  RegisterListen(true);
#else
  RegisterListen(false);
#endif
}

void loop() {
#if Sender
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
#else
#endif
}
