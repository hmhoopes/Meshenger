/*
 * LocalESP BLE serial bridge - Nordic UART Service (NUS).
 * Use with the Meshenger web app: connect via Web Bluetooth to send/receive messages.
 *
 * NUS UUIDs (same as web app):
 *   Service:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX (write from client): 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 *   TX (notify to client):  6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 */
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_NAME "Meshenger-LocalESP"
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Buffer for data to send to the connected client (e.g. from Serial or mesh later)
#define TX_BUF_SIZE 512
uint8_t txBuf[TX_BUF_SIZE];
int txLen = 0;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE client disconnected");
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string rx = pCharacteristic->getValue();
    if (rx.length() > 0) {
      Serial.print("[BLE RX] ");
      for (size_t i = 0; i < rx.length(); i++) {
        Serial.print((char)rx[i]);
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Meshenger LocalESP - BLE NUS");
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
  Serial.println("Advertising as \"" DEVICE_NAME "\"");
}

void loop() {
  // Send any pending data from Serial to the BLE client (for future: mesh -> BLE)
  while (Serial.available() && deviceConnected && txLen < TX_BUF_SIZE) {
    txBuf[txLen++] = (uint8_t)Serial.read();
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

  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Advertising restarted");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(20);
}
