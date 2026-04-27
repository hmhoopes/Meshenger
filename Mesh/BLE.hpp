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
#include <Update.h>

//utility headers
#include <span>
#include <string>
#include <vector>

// Mesh types and externs when built with Mesh (Node); stubs when Pager builds standalone.
#include "../Mesh/Peer.hpp"

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
// Dedicated OTA bulk-data characteristic: WRITE_NR only, separate from CHARACTERISTIC_RX
// so having both WRITE and WRITE_NR on the same characteristic doesn't break the ATT handler.
#define CHARACTERISTIC_OTA  "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_BUF_SIZE 512

// BLE global variables:
// BLE server/characteristic pointers and connection/advertising/send flags.
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLECharacteristic* pOtaCharacteristic = NULL;
bool deviceConnected = false;
bool isAdvertising = false;
bool sendToMesh = false;
bool otaMode = false;
bool otaError = false;
size_t otaExpectedSize = 0;
size_t otaWritten = 0;

// Helpers
#include "../Mesh/Helpers.hpp"

// Advertise:
// Start BLE advertising if no client is connected and advertising isn't already active.
void Advertise(){
  if (isAdvertising || deviceConnected) {
    return;
  }

  pServer->startAdvertising();
#ifdef SERIAL_LOG_DEBUG
  Serial.println("[BLE] Advertising restarted");
#endif
  isAdvertising = true;
}

// ServerCallbacks::onConnect/onDisconnect:
// BLE server callback hooks to track connection state and restart advertising on disconnect.
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    isAdvertising = false;
#ifdef SERIAL_LOG_DEBUG
    Serial.println();
    Serial.println("[BLE] Client connected");
#endif
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Advertise();
#ifdef SERIAL_LOG_DEBUG
    Serial.println("[BLE] Client disconnected");
#endif
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

String messageToSend;
bool messagePending = false;
MAC targetMAC = MAC(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

void ResetMessage(){
  messageToSend = "";
  messagePending = false;
  MAC targetMAC = MAC(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

void SendMessage(){
#ifdef SERIAL_LOG_DEBUG
  Serial.print("sending message: ");
  Serial.println(messageToSend);
#endif
  SendTextMessage(targetMAC, messageToSend);
  ResetMessage();
}

// HandleOTA:
// Process OTA firmware update commands sent over BLE.
// 'ob' + 8-hex-char size = begin, 'od' + binary data = chunk, 'oe' = commit and reboot.
// Requires pager built with an OTA-capable partition scheme (e.g. min_spiffs).
void HandleOTA(const String& rx_val) {
  if (rx_val.length() < 2) return;
  const char subcmd = rx_val[1];

  if (subcmd == 'b') {
    // begin: "ob" + 8 ASCII hex chars representing total firmware size
    if (rx_val.length() < 10) return;
    otaExpectedSize = (size_t)strtoul(rx_val.substring(2, 10).c_str(), nullptr, 16);
    otaWritten = 0;
    otaError = false;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      std::string err = std::string("oerr:") + Update.errorString() + "\n";
      SendToApp(std::as_bytes(std::span<char>(reinterpret_cast<char*>(err.data()), err.size())));
#ifdef SERIAL_LOG_DEBUG
      Serial.printf("[OTA] begin failed: %s\n", Update.errorString());
#endif
      return;
    }
    otaMode = true;
    std::string ok = "ook\n";
    SendToApp(std::as_bytes(std::span<char>(reinterpret_cast<char*>(ok.data()), ok.size())));
#ifdef SERIAL_LOG_DEBUG
    Serial.printf("[OTA] Begin: expecting %u bytes\n", (unsigned)otaExpectedSize);
#endif

  } else if (subcmd == 'd') {
    // data chunk: "od" + raw binary firmware bytes
    if (!otaMode || otaError) return;
    const size_t dataLen = rx_val.length() - 2;
    if (dataLen == 0) return;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(rx_val.c_str()) + 2;
    const size_t written = Update.write(const_cast<uint8_t*>(data), dataLen);
    if (written != dataLen) {
      otaError = true;
#ifdef SERIAL_LOG_DEBUG
      Serial.printf("[OTA] write error: %s\n", Update.errorString());
#endif
    } else {
      otaWritten += written;
    }

  } else if (subcmd == 'e') {
    // end: commit the update and reboot
    if (!otaMode) return;
    otaMode = false;
    if (otaError) {
      Update.abort();
      std::string err = "oerr:Write failed during transfer\n";
      SendToApp(std::as_bytes(std::span<char>(reinterpret_cast<char*>(err.data()), err.size())));
      return;
    }
    if (!Update.end(true)) {
      std::string err = std::string("oerr:") + Update.errorString() + "\n";
      SendToApp(std::as_bytes(std::span<char>(reinterpret_cast<char*>(err.data()), err.size())));
#ifdef SERIAL_LOG_DEBUG
      Serial.printf("[OTA] end failed: %s\n", Update.errorString());
#endif
      return;
    }
    std::string ok = "ook\n";
    SendToApp(std::as_bytes(std::span<char>(reinterpret_cast<char*>(ok.data()), ok.size())));
#ifdef SERIAL_LOG_DEBUG
    Serial.println("[OTA] Complete — rebooting");
#endif
    delay(2000); // give Chrome time to receive the ook notification before the device disappears
    ESP.restart();
  }
}

// OtaDataCallbacks::onWrite:
// Receives raw firmware binary chunks on the dedicated OTA characteristic (WRITE_NR).
// Writes directly into the Update stream; no command prefix — pure binary data.
class OtaDataCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (!otaMode || otaError) return;
    auto rx_val = pCharacteristic->getValue();
    const size_t dataLen = rx_val.length();
    if (dataLen == 0) return;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(rx_val.c_str());
    const size_t written = Update.write(const_cast<uint8_t*>(data), dataLen);
    if (written != dataLen) {
      otaError = true;
#ifdef SERIAL_LOG_DEBUG
      Serial.printf("[OTA] write error: %s\n", Update.errorString());
#endif
    } else {
      otaWritten += written;
#ifdef SERIAL_LOG_DEBUG
      Serial.printf("[OTA] written %u / %u bytes\n", (unsigned)otaWritten, (unsigned)otaExpectedSize);
#endif
    }
  }
};

// RxCallbacks::onWrite:
// Handler for incoming BLE writes from the client (RX characteristic). Forwards data to mesh when enabled.
class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    auto rx_val = pCharacteristic->getValue();
    std::string rx = std::string(rx_val.c_str());
#ifdef SERIAL_LOG_DEBUG
    Serial.print("[RX] Received from app: ");
    Serial.println(rx.c_str());
#endif
    if (rx.empty()) {
        return;
    }

    if (rx_val[0] == 'o') {
      HandleOTA(rx_val);
      return;
    }

    if (messagePending){
#ifdef SERIAL_LOG_DEBUG
      Serial.print("[RX] adding to message: ");
      Serial.println(rx.c_str());
#endif
      auto idx = rx_val.indexOf(0x03);
      if (idx != -1){
        rx_val = rx_val.substring(0, idx); // Remove the delimiter from the text
        messageToSend += rx_val;
        SendMessage();
      } else {
        messageToSend += rx_val;
      }
      if (messageToSend.length() > MessageSize) {
#ifdef SERIAL_LOG_DEBUG
        Serial.println("ERR: Message exceeds max length, clearing out message ...");
#endif
        ResetMessage();
      }
    } else if (rx[0] == 'm') {
      // Message from app to send to mesh
      rx = rx.substr(1);  // Remove message type prefix
      messagePending = true;

      size_t len = rx_val.length();
      if (len > 0) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(rx_val.c_str());
#ifdef SERIAL_LOG_DEBUG
        Serial.print("[RX] ");
        SerialPrintPrintable(p, len);
        Serial.println();
#endif
      }

      if(sendToMesh) {
#ifdef SERIAL_LOG_DEBUG
        Serial.println("[MESH] Sending text to mesh...");
#endif
        rx_val.remove(0,1);  // Remove the 'm' prefix before sending
        static constexpr auto targetPeerSize = 17;

        if (rx_val.length() < targetPeerSize) {
#ifdef SERIAL_LOG_DEBUG
          Serial.println("ERR: Received message too short to contain target peer MAC");
#endif
          return;
        } else {
          String targetPeerStr = rx_val.substring(0, targetPeerSize);
          MAC tempMac(std::vector<uint8_t>{
            (uint8_t)strtoul(targetPeerStr.substring(0, 2).c_str(), nullptr, 16),
            (uint8_t)strtoul(targetPeerStr.substring(3, 5).c_str(), nullptr, 16),
            (uint8_t)strtoul(targetPeerStr.substring(6, 8).c_str(), nullptr, 16),
            (uint8_t)strtoul(targetPeerStr.substring(9, 11).c_str(), nullptr, 16),
            (uint8_t)strtoul(targetPeerStr.substring(12, 14).c_str(), nullptr, 16),
            (uint8_t)strtoul(targetPeerStr.substring(15, 17).c_str(), nullptr, 16)
          });
#ifdef SERIAL_LOG_DEBUG
          Serial.print("Parsed temp MAC: ");
          Serial.println(tempMac.to_cstr());
#endif
          targetMAC = tempMac;
#ifdef SERIAL_LOG_DEBUG
          Serial.print("Parsed target MAC: ");
          Serial.println(targetMAC.to_cstr());
#endif
        }
        auto text = rx_val.substring(targetPeerSize);
        auto idx = text.indexOf(0x03);
        if (idx != -1){
          text = text.substring(0, idx); // Remove the delimiter from the text
          messageToSend += text;
          SendMessage();
        }
        messageToSend += text;
      }
#ifdef SERIAL_LOG_DEBUG
      Serial.print("Still sending message?");
      Serial.println((messagePending) ? "true" : "false");
#endif
    } else if (rx[0] == 'l') {
      // Command from app to list peers
      rx = "";  // No additional data needed
      Peer targetPeer = Peer(MAC(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));
      SendToApp(PeersJSON());
    } else if (rx[0] == 's') {
      // command from app to set 12 char name
      rx_val.remove(0,1);  // Remove the 'm' prefix before sending

      size_t len = rx_val.length();
      if (len > 12) {
        len = 12; // Truncate to 12 chars if longer
      }
      deviceUsername = rx_val.substring(0, len);
#ifdef SERIAL_LOG_DEBUG
      Serial.print("Setting device username to: ");
      Serial.println(deviceUsername);
#endif
    } else {
#ifdef SERIAL_LOG_DEBUG
        Serial.println("ERR: Received unknown command from BLE client");
#endif
    }
  }
};

// InitializeBLE:
// Initialize the BLE stack, create the NUS service and RX/TX characteristics,
// set callbacks, and start advertising with the provided name suffix.
void InitializeBLE(String aName){
  InitializeSerial();
#ifdef SERIAL_LOG_DEBUG
  Serial.println();
  Serial.println("========== Meshenger Pager ==========");
  Serial.println("BLE NUS - connect with web app or any NUS client");
  Serial.println("=====================================");
#endif

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

  pOtaCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_OTA,
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  pOtaCharacteristic->setCallbacks(new OtaDataCallbacks());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  isAdvertising = true;
#ifdef SERIAL_LOG_DEBUG
  Serial.println("[BLE] Advertising as \"" DEVICE_NAME "\"");
  Serial.println();
#endif
}

// IsConnected:
// Return true if a BLE client is currently connected to the pager.
bool IsConnected() {return deviceConnected; }

//Note: message won't be sent if not connected
// SendToApp:
// Send raw byte data to the connected BLE client via notifications,
// chunking the payload to TX_BUF_SIZE and flushing on newline.
void SendToApp(const std::span<const std::byte> aData){
#ifdef SERIAL_LOG_DEBUG
  Serial.println("[TX] Preparing to send to app ...");
#endif
  size_t dataLen = 0;
  const size_t dataSize = aData.size();
  if (dataSize == 0 || !deviceConnected) return;

#ifdef SERIAL_LOG_DEBUG
  Serial.print("[TX] Sending to app (");
  Serial.print(dataSize);
  Serial.println(" bytes)");
#endif

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
#ifdef SERIAL_LOG_DEBUG
  Serial.println("[TX] Sent to app");
#endif
}

#endif
