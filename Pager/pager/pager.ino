// Custom library headers
#include "../BLE.hpp"
#include "../../Mesh/Helpers.hpp"

#include <memory>

void setup() {
  isPager = true;
  sendToMesh = true;
  InitializeESPNow(); 
  InitializeBLE(GetMACAddress().to_arduinostr());
}

void loop() {

}
