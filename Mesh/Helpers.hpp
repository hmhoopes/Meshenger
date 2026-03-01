#ifndef MESH_HELPERS_HPP
#define MESH_HELPERS_HPP 

#include "MAC.hpp"
#include "Message.hpp"
#include "Peer.hpp"

// WiFi & ESP Headers
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>

// Utility Headers
#include <vector>
#include <string>
#include <algorithm>
#include <optional>
#include <assert.h>

//================================== Forward Decls ===============================================
void OnSenderReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len);
void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len);
//================================================================================================

// Broadcast MAC is a way to send messages to all devices.
//   This does not change across all devices
MAC BroadcastMAC = MAC(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
Peer BroadcastPeer = Peer(BroadcastMAC);
std::vector<Peer> Peers;

// ╔════════════════════════════╗
// ║  Generic Helpers and Init  ║
// ╚════════════════════════════╝

void InitializeSerial(){
  // Initialize Serial Monitor
  if (!Serial){
    Serial.begin(115200);
  }
}

void InitializeESPNow(){
  InitializeSerial();
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERR: Could not initalize ESP-NOW");
    assert(false);
  }

  if (!BroadcastPeer.AddPeer()){
    Serial.println("ERR: Failed to add broadcast as peer");
    assert(false);
  }
}

// Register callbacks
void RegisterListen() {
  esp_now_register_recv_cb(OnDataReceive);
}

// Send out a discovery message
void AnnouceMAC(){
  Message message;
  message.info[0] = '\0';
  message.type = MessageType::Discovery;
  message.id = 0;
  
  bool success = SendMessage(BroadcastPeer, message);
  Serial.println((success) ? "Annouced successfully" : "ERR: couldn't send message");
}

std::optional<Peer> FindPeer(MAC source) {
  auto it = std::find_if(Peers.begin(), Peers.end(), [&](const Peer& p) {
      return p.GetMAC() == source;
  });

  if (it != Peers.end()) {
      return *it;
  }

  return std::nullopt;
}

void HandleDiscovery(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
#ifdef DEBUG
  Serial.print("DBG: Recieved Discovery Message from: ");
  Serial.println(source.to_cstr());
#endif

  // Add peer
  Peer source_peer = Peer(source);

  if (!source_peer.AddPeer()){
    Serial.println("ERR: Failed to add discovery sender as peer");
    return;
  }

  if (!source_peer.PrevAdded()){
    Peers.emplace_back(source_peer);
  }

  //Send reply
  if (message.id == 0){
    Message message;
    message.info[0] = '\0';
    message.type = MessageType::DiscoveryResponse;
    message.id = 1;
    bool success = SendMessage(source_peer, message);
    Serial.println((success) ? "Replied successfully" : "ERR: couldn't send message");
  }
}

void HandleSenderDiscoveryResponse(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
#ifdef DEBUG
  Serial.print("DBG: Recieved DiscoveryResponse Message from: ");
  Serial.println(source.to_cstr());
#endif

  // Add peer
  Peer source_peer = Peer(source);

  if (!source_peer.AddPeer()){
    Serial.println("ERR: Failed to add ACK sender as peer");
    return;
  }

  if (!source_peer.PrevAdded()){
    Peers.emplace_back(source_peer);
  }
}

void HandleACK(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
#ifdef DEBUG
  Serial.print("DBG: Recieved ACK Message from: ");
  Serial.println(source.to_cstr());
#endif
  
  if (message.id == ackId) {
    waitingForAck = false;
  } else {
    Serial.println("ERR: ACK id did not match message");
  }
}

void HandleText(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
  auto source_peer = FindPeer(source);

  if (source_peer.has_value()) {
    Message ack_message;
    ack_message.type = MessageType::ACK;
    ack_message.id = message.id;
    SendMessage(*source_peer, ack_message);
  } else {
    Serial.println("ERR: Source peer has not been added.");
  }
}

void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len) {
  if (len != sizeof(Message)){
    Serial.println("ERR: received message of different size than expected");
    return;
  }

  Message message;
  memcpy(&message, incomingData, sizeof(Message));

  switch (message.type){
    case MessageType::ACK:
      HandleACK(info, message);
      break;
    case MessageType::DiscoveryResponse:
      HandleSenderDiscoveryResponse(info, message);
      break;
    case MessageType::Discovery:
      HandleDiscovery(info, message);
      break;
    case MessageType::Text:
      HandleText(info, message);
    case MessageType::Invalid:
    default:
      Serial.print("Message Recieved: ");
      Serial.println(message.to_cstr());
      break;
  }
}


bool SendTextMessage(Peer receiver, String msg) {
  Message message;
  message.type = MessageType::Text;
  bool success = true;

  // Iterate over entire message
  for (int i = 0; msg.length() > i * MessageSize; i++) {
    // Copy the message plus some offset up to message size
    strncpy(message.info, msg.c_str() + i * MessageSize, MessageSize);  // Prevents buffer overflow
    message.id = i;
#ifdef DEBUG
    Serial.println("DBG: Sending message");
#endif
    success = SendMessageWithRetry(receiver, message) && success;
    if (!success) {
      Serial.println("ERR: Failed to send text message.");
    }
  }

  return success;
}

#endif
