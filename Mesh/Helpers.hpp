/*
Project: Meshenger
Module Name: Helpers.hpp
Description:
    Utility and initialization helpers for ESP-NOW mesh communication,
    message handling, peer management and pager mode behavior.
Inputs:
    - esp_now_recv_info pointers, Message structs, MAC addresses,
      Peer objects, and text strings to various helper functions.
Outputs:
    - Sends/receives messages over ESP-NOW, maintains peer list,
      forwards data to BLE when acting as a pager.
External Sources:
    - esp_now, WiFi, BLE libraries, STL containers (vector, optional, etc.)
Author: Team 2
Creation Date: 02/11/2026
*/

#ifndef MESH_HELPERS_HPP
#define MESH_HELPERS_HPP 

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
#include <span>

// Helper files
#include "MAC.hpp"
#include "Message.hpp"
#include "Peer.hpp"

// forward decls
std::span<const std::byte> PeersJSON();
void SetPagerMode();
void InitializeSerial();
void RegisterListen();
void InitializeESPNow();
void AnnouceMAC();
std::optional<Peer> FindPeer(MAC source);
void HandleDiscovery(const esp_now_recv_info* info, const Message message) ;
void HandleSenderDiscoveryResponse(const esp_now_recv_info* info, const Message message) ;
void HandleACK(const esp_now_recv_info* info, const Message message) ;
void HandleText(const esp_now_recv_info* info, const Message message) ;
void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len) ;
bool SendTextMessage(Peer receiver, String msg) ;
extern Peer BroadcastPeer;

#include "../Pager/BLE.hpp"

//================================== Forward Decls ===============================================
void OnSenderReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len);
void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len);
//================================================================================================

// isPager:
// Flag indicating whether this device acts as a BLE pager (forwards mesh messages to BLE client).
bool isPager = false;

// SetPagerMode:
// Configure device to behave as a pager and enable forwarding between BLE and mesh.
void SetPagerMode(){
  isPager = true;
  sendToMesh = true;
}

// BroadcastMAC & BroadcastPeer:
// Preconfigured broadcast MAC/Peer used to send discovery messages to all nodes.
MAC BroadcastMAC = MAC(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
Peer BroadcastPeer = Peer(BroadcastMAC);

// Peers:
// Local list of discovered/known peers.
std::vector<Peer> Peers;

//Function to get a JSON-formatted list of peer MAC addresses for sending to BLE client
std::span<const std::byte> PeersJSON() {
  Serial.println("Generating peers JSON...");
  Serial.print("Peers Size: ");
  Serial.println(Peers.size());
  //include the 'l' command prefix to indicate this is a peer list response
  std::string json = "l[";
  for (size_t i = 0; i < Peers.size(); i++) {
    json += "\"" + std::string(Peers[i].GetMAC().to_cstr()) + "\"";
    if (i < Peers.size() - 1) {
      json += ",";
    }
  }
  json += "]";
  Serial.print("Generated JSON: ");
  Serial.println(json.c_str());
  return std::as_bytes(std::span<char>(reinterpret_cast<char *>(json.data()), json.size()));
}

// InitializeSerial:
// Ensure serial I/O is initialized (115200) for logging and debug output.
void InitializeSerial(){
  // Initialize Serial Monitor
  if (!Serial){
    Serial.begin(115200);
  }
}

// RegisterListen:
// Register the ESP-NOW receive callback so that OnDataReceive is invoked for incoming packets.
void RegisterListen() {
  esp_now_register_recv_cb(OnDataReceive);
}

// InitializeESPNow:
// Configure WiFi mode, initialize ESP-NOW, add broadcast peer and register receive callbacks.
// Aborts on critical failures.
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

  RegisterListen();
}

// AnnouceMAC:
// Broadcast a discovery message to the mesh to announce this device's presence.
void AnnouceMAC(){
  Message message;
  message.info[0] = '\0';
  message.type = MessageType::Discovery;
  message.id = 0;
  
  bool success = SendMessage(BroadcastPeer, message);
  Serial.println((success) ? "Annouced successfully" : "ERR: couldn't send message");
}

// FindPeer:
// Look up a Peer in the local Peers list by MAC address; returns optional<Peer>.
std::optional<Peer> FindPeer(MAC source) {
  auto it = std::find_if(Peers.begin(), Peers.end(), [&](const Peer& p) {
      return p.GetMAC() == source;
  });

  if (it != Peers.end()) {
      return *it;
  }

  return std::nullopt;
}

// HandleDiscovery:
// Handle incoming Discovery messages: add sender to local peer list and reply when appropriate.
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

// HandleSenderDiscoveryResponse:
// Handle incoming DiscoveryResponse messages: add sender to local peer list if absent.
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

// HandleACK:
// Handle incoming ACK messages: clear waitingForAck when ACK matches expected ackId.
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

// HandleText:
// Handle incoming Text messages: send ACK back to sender if known.
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

  if (isPager){
    //TODO: improve the format sent to end user
    Serial.println("receiving message...");
    size_t len = strnlen(message.info, MessageSize);
    SendToApp(std::as_bytes(std::span<const char>(message.info, len)));
  }
}

// OnDataReceive:
// Entry point for received ESP-NOW payloads: validate size, deserialize Message and dispatch to handlers.
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

// SendTextMessage:
// Split a long text string into MessageSize chunks and send each chunk as a Text
// message using SendMessageWithRetry; returns overall success.
bool SendTextMessage(Peer receiver, String msg) {
  Message message;
  message.type = MessageType::Text;
  bool success = true;
  printf("Sending text message: %s\n", msg.c_str());

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
