// Custom library headers
#include "../BLE.hpp"
#include "../../Mesh/Helpers.hpp"

#include <memory>

/*
TODO:
  - fix SendTextMessage repeating (Aniketh)
    - attempting to get ack
  - workout why SendToApp sends junk bytes sometimes (Shero & Yaeesh)
  - workout how to send to mesh asynchronously (do in later sprint)
  - add target selection (do in later sprint)
  - add node propagation (do in later sprint)
    - add queue of message hashes?
    - drop if seen before, otherwise broadcast
*/

void setup() {
  SetPagerMode();
  InitializeESPNow(); 
  InitializeBLE(GetMACAddress().to_arduinostr());
}

void loop() {
#ifdef SENDTOAPP_DEBUG
  std::string message = "hellooooooooooo ";
  if (IsConnected()){
    SendToApp(
      std::as_bytes(
        std::span<char>(reinterpret_cast<char *>(message.data()), message.size())
      )
    );
  }
  delay(1000);
#endif
}
