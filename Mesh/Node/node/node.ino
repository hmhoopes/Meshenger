#include "Helpers.h"

std::vector<MAC> MAC_addresses;

//0 for receiver, 1 for sender
#define Sender 0

void setup() {
  InitializeESPNow();
#if Sender
#else
  RegisterListen();
#endif
}

void loop() {
#if Sender
  Broadcast();
  delay(2000);
#else
#endif
}
