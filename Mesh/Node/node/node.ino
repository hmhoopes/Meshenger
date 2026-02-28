#include "../../Helpers.hpp"

void setup() {
  InitializeESPNow();
  RegisterListen();
}

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
