// BLE headers
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Helpers
#include "../Mesh/Helpers.hpp"

//utility headers
#include <span>

//BLE constants
#define DEVICE_NAME "Meshenger-Pager"
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_BUF_SIZE 512

//BLE global variables
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool isAdvertising = false;
bool sendToMesh = false;

void Advertise(){
  if (isAdvertising || deviceConnected) {
    return;
  }

  pServer->startAdvertising();
  Serial.println("Advertising restarted");
  isAdvertising = true;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    isAdvertising = false;
    Serial.println("BLE client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Advertise();
    Serial.println("BLE client disconnected\n---------\n---------\n---------\n---------\n---------");
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (sendToMesh){
      // implement
    } else {
      std::string rx = std::string(pCharacteristic->getValue().c_str());
      if (rx.length() > 0) {
        Serial.print("[BLE RX] ");
        for (size_t i = 0; i < rx.length(); i++) {
          Serial.print((char)rx[i]);
        }
      }
    }     
  }
};

void InitializeBLE(){
  InitializeSerial();
  
  Serial.println("Meshenger Pager - BLE NUS");
  Serial.println("Connect with the web app (Chrome) or any NUS client.");

  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new RxCallbacks());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  isAdvertising = true;
  Serial.println("Advertising as \"" DEVICE_NAME "\"");
}

bool IsConnected() {return deviceConnected; }

//Note: message won't be sent if not connected
void SendToApp(const std::span<const std::byte> aData){
  // Buffer for data to send to the connected client (e.g. from Serial or mesh later)
  uint8_t txBuf[TX_BUF_SIZE];
  int txLen = 0;
  int dataLen = 0;
  // Send any pending data from Serial to the BLE client (for future: mesh -> BLE)
  while (deviceConnected && txLen < aData.size()) {
    txBuf[txLen++] = (uint8_t)aData[dataLen++];
    if (txLen >= TX_BUF_SIZE || txBuf[txLen - 1] == '\n') {
      pTxCharacteristic->setValue(txBuf, txLen);
      pTxCharacteristic->notify();
      txLen = 0;
    }
  }
  if (txLen > 0 && deviceConnected) {
    pTxCharacteristic->setValue(txBuf, txLen);
    pTxCharacteristic->notify();
    txLen = 0;
  }
}