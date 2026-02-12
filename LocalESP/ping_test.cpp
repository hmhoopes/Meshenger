// tests pinging the ESP32 board over bluetooth, either BTLE or BT Classic 
// notably LE is much lighter weight on data but is only bt 4.0 or later, data is at much slower rates than classic 
// if we are sending only text data btle is probably fine 


//include the bt serial library -- need to get this from somewhere 
#include "BluetoothSerial.h"


//make sure bt is enabled in the config 

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  
  
  SerialBT.begin("ESP32_Android_Ping"); 
  
  Serial.println("started ");
}

void loop() {
  // Check if data is coming from the Phone
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    incoming.trim(); // Remove any extra spaces/newlines

    Serial.print("message: ");
    Serial.println(incoming);

    
    if (incoming.equalsIgnoreCase("ping")) {
      SerialBT.println("PONG! (Mesh Gateway Ready)");
      Serial.println("Sent response to phone.");
    } else {
      SerialBT.println("I only respond to 'ping'!");
    }
  }
  delay(20);
}