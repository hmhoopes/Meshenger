/*
Project: Meshenger
Module Name: BLE.hpp
Description:
    BLE Nordic UART Service implementation used by pager devices.
    Manages advertising, connection state, RX/TX characteristics,
    and forwarding between BLE and mesh.
Inputs:
    - Name suffix for BLE device, byte spans from mesh or serial.
Outputs:
    - BLE notifications, mesh forwarding, connection callbacks.
External Sources:
    - BLEDevice, BLEServer, BLEUtils, BLE2902 libraries.
Author: Team 2
Creation Date: 02/28/2026
*/

#ifndef PAGER_BLE_HPP
#define PAGER_BLE_HPP

// BLE headers
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

//utility headers
#include <span>
#include <string>
#include <vector>

// Mesh types and externs when built with Mesh (Node); stubs when Pager builds standalone.
#include "../Mesh/Peer.hpp"
#ifdef PAGER_BLE_STANDALONE
// Stubs so Pager/ble_serial.ino builds without Mesh/Helpers.hpp
void InitializeSerial() { if (!Serial) Serial.begin(115200); }
static MAC _broadcastMAC = MAC(std::vector<uint8_t>{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF});
Peer BroadcastPeer = Peer(_broadcastMAC);
bool SendTextMessage(Peer receiver, String msg) { (void)receiver; (void)msg; return true; }
#else
extern Peer BroadcastPeer;
bool SendTextMessage(Peer receiver, String msg);
#endif

//forward decls
void Advertise();
void InitializeBLE(String aName);
bool IsConnected();
void SendToApp(const std::span<const std::byte> aData);

// BLE constants:
// Device name and NUS service/characteristic UUIDs used for the pager BLE service.
#define DEVICE_NAME "Meshenger-Pager"
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_BUF_SIZE 512

// BLE global variables:
// BLE server/characteristic pointers and connection/advertising/send flags.
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool isAdvertising = false;
bool sendToMesh = false;

// Advertise:
// Start BLE advertising if no client is connected and advertising isn't already active.
void Advertise(){
  if (isAdvertising || deviceConnected) {
    return;
  }

  pServer->startAdvertising();
  Serial.println("[BLE] Advertising restarted");
  isAdvertising = true;
}

// ServerCallbacks::onConnect/onDisconnect:
// BLE server callback hooks to track connection state and restart advertising on disconnect.
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    isAdvertising = false;
    Serial.println();
    Serial.println("[BLE] Client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Advertise();
    Serial.println("[BLE] Client disconnected");
  }
};

// Helper: print only printable ASCII to Serial (avoids garbage from binary/control chars).
static void SerialPrintPrintable(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint8_t c = data[i];
    if (c >= 0x20 && c <= 0x7E || c == '\t' || c == '\n' || c == '\r')
      Serial.print((char)c);
  }
}

// RxCallbacks::onWrite:
// Handler for incoming BLE writes from the client (RX characteristic). Forwards data to mesh when enabled.
class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    auto rx_val = pCharacteristic->getValue();
    size_t len = rx_val.length();
    if (len > 0) {
      const uint8_t* p = (const uint8_t*)rx_val.data();
      Serial.print("[RX] ");
      SerialPrintPrintable(p, len);
      Serial.println();
    }

    if(sendToMesh) {
      Serial.println("[MESH] Sending text to mesh...");
      SendTextMessage(BroadcastPeer, String(rx_val.c_str()));
    }
  }
};

// InitializeBLE:
// Initialize the BLE stack, create the NUS service and RX/TX characteristics,
// set callbacks, and start advertising with the provided name suffix.
void InitializeBLE(String aName){
  InitializeSerial();
  Serial.println();
  Serial.println("========== Meshenger Pager ==========");
  Serial.println("BLE NUS - connect with web app or any NUS client");
  Serial.println("=====================================");

  BLEDevice::init(DEVICE_NAME + aName);
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
  Serial.println("[BLE] Advertising as \"" DEVICE_NAME "\"");
  Serial.println();
}

// IsConnected:
// Return true if a BLE client is currently connected to the pager.
bool IsConnected() {return deviceConnected; }

//TODO: workout why it prints weird sometimes
//Note: message won't be sent if not connected
// SendToApp:
// Send raw byte data to the connected BLE client via notifications,
// chunking the payload to TX_BUF_SIZE and flushing on newline.
void SendToApp(const std::span<const std::byte> aData){
  size_t dataLen = 0;
  const size_t dataSize = aData.size();
  if (dataSize == 0 || !deviceConnected) return;

  Serial.print("[TX] Sending to app (");
  Serial.print(dataSize);
  Serial.println(" bytes)");

  uint8_t txBuf[TX_BUF_SIZE];
  int txLen = 0;

  while (deviceConnected && dataLen < dataSize) {
    txBuf[txLen++] = (uint8_t)aData[dataLen++];
    if (txLen >= TX_BUF_SIZE || (txLen > 0 && txBuf[txLen - 1] == '\n')) {
      pTxCharacteristic->setValue(txBuf, txLen);
      pTxCharacteristic->notify();
      txLen = 0;
    }
  }
  if (txLen > 0 && deviceConnected) {
    pTxCharacteristic->setValue(txBuf, txLen);
    pTxCharacteristic->notify();
  }
}

#endif
