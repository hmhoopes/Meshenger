#include "../../Helpers.hpp"

void setup() {
  InitializeESPNow();
  RegisterListen();
}

void loop() {
  AnnouceMAC();
  Serial.println("DBG: Annouced");
  delay(2000);
  if (Peers.size() != 0){
    Serial.println("DBG: Getting front");
    Peer receiver = Peers.front();
    Serial.println("DBG: Making message");
    Message message;
    strncpy(message.info, "Hello", MessageSize - 1);  // Prevents buffer overflow
    message.info[MessageSize - 1] = '\0';  // Ensure null termination
    message.type = MessageType::Text;
    message.id = 1;
    Serial.println("DBG: Sending message");
    bool success = SendMessage(receiver, message);
    if (!success) {
      Serial.println("Could not send text message.");
    }
  }
  delay(2000);
}
