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

//
// ╔════════════════════════════╗
// ║  Generic Helpers and Init  ║
// ╚════════════════════════════╝

void InitializeESPNow(){
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    assert(false);
  }

  if (!BroadcastPeer.AddPeer()){
    Serial.println("Failed to add broadcast as peer");
    assert(false);
  }
}

// Register different callbacks for sender/receiver
void RegisterListen(bool isSender) {
  if (isSender) {
    esp_now_register_recv_cb(OnSenderReceive);
  } else {
    esp_now_register_recv_cb(OnDataReceive);
  }
}

// Send out a discovery message
void AnnouceMAC(){
  Message message;
  message.info[0] = '\0';
  message.type = MessageType::Discovery;
  message.id = 0;
  
  bool success = SendMessage(BroadcastPeer, message);
  Serial.println((success) ? "Annouced successfully" : "Error: couldn't send message");
}


// ╔═══════════════════════════════════════╗
// ║  Handlers and Callbacks for Receiver  ║
// ╚═══════════════════════════════════════╝

void HandleDiscovery(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
  Serial.print("Recieved Discovery Message from: ");
  Serial.println(source.to_cstr());

  //Add peer
  Peer source_peer = Peer(source);

  if (!source_peer.AddPeer()){
    Serial.println("failed to add discovery sender as peer");
    return;
  }

  if (!source_peer.PrevAdded()){
    Peers.emplace_back(source_peer);
  }

  if (message.id == 0){
    //Send reply
    Message message;
    message.info[0] = '\0';
    message.type = MessageType::DiscoveryResponse;
    message.id = 1;
    bool success = SendMessage(source_peer, message);
    Serial.println((success) ? "Replied successfully" : "Error: couldn't send message");
  }
}

void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len) {
  if (len != sizeof(Message)){
    Serial.println("Error: received message of different size than expected");
    return;
  }

  Message message;
  memcpy(&message, incomingData, sizeof(Message));

  switch (message.type){
    case MessageType::Discovery:
      HandleDiscovery(info, message);
      break;
    case MessageType::Text:
      Serial.print("New Message: ");
      Serial.println(message.info);
      break;
    case MessageType::Invalid:
    default:
      Serial.print("Message Info: ");
      Serial.println(message.info);
      Serial.print("Message Type: ");
      Serial.println(message.type);
      Serial.print("Message ID: ");
      Serial.println(message.id);
      break;
  }
}


// ╔═════════════════════════════════════╗
// ║  Handlers and Callbacks for Sender  ║
// ╚═════════════════════════════════════╝

void HandleSenderDiscoveryResponse(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
  Serial.print("Recieved DiscoveryResponse Message from: ");
  Serial.println(source.to_cstr());

  //Add peer
  Peer source_peer = Peer(source);

  if (!source_peer.AddPeer()){
    Serial.println("failed to add ACK sender as peer");
    return;
  }

  if (!source_peer.PrevAdded()){
    Peers.emplace_back(source_peer);
  }
}

void OnSenderReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len) {
  if (len != sizeof(Message)){
    Serial.println("Error: received message of different size than expected");
    return;
  }

  Message message;
  memcpy(&message, incomingData, sizeof(Message));

  switch (message.type) {
    case MessageType::DiscoveryResponse:
      HandleSenderDiscoveryResponse(info, message);
      break;
    case MessageType::Invalid:
    default:
      MAC source = GetSenderMAC(info);
      Serial.print("Message From: ");
      Serial.println(source.to_cstr());
      Serial.print("Message Info: ");
      Serial.println(message.info);
      Serial.print("Message Type: ");
      Serial.println(message.type);
      Serial.print("Message ID: ");
      Serial.println(message.id);
      break;
  }
}

#endif