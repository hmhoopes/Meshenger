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
#include <utility>

// Helper files
#include "MAC.hpp"
#include "Peer.hpp"

extern MAC BroadcastMAC;
extern Peer BroadcastPeer;

// Helper files cont'd
#include "Message.hpp"

// forward decls
std::span<const std::byte> PeersJSON();
void SetPagerMode();
void InitializeSerial();
void RegisterListen();
void InitializeESPNow();
void AnnouceMAC();
std::optional<Peer> FindPeer(MAC source);
std::optional<Peer> FindNextHop(MAC dst);
void HandleDiscovery(const esp_now_recv_info* info, const Message message);
void HandleSenderDiscoveryResponse(const esp_now_recv_info* info, const Message message);
void HandleACK(const esp_now_recv_info* info, const Message message);
void HandleText(const esp_now_recv_info* info, const Message message);
void HandlePeerList(const esp_now_recv_info* info, const Message message);
void SendPeerList(Peer target);
void PruneStale();
void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len);
bool SendTextMessage(MAC receiver, String msg);

#include "BLE.hpp"

//================================== Forward Decls ===============================================
void OnSenderReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len);
void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len);
//================================================================================================

// 12 character username for messaging
String deviceUsername = "default-name";

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

// SelfMAC:
// This device's own MAC address; initialized in InitializeESPNow.
MAC SelfMAC;

// Peers:
// Local list of directly reachable (single-hop) peers.
std::vector<Peer> Peers;

// RoutingTable:
// Maps a destination MAC to the next-hop MAC for multi-hop delivery.
// Populated when a direct neighbor shares its peer list via PeerList messages.
std::vector<std::pair<MAC, MAC>> RoutingTable;

// PEER_TIMEOUT_MS:
// How long (ms) without a discovery heartbeat before a peer is considered gone.
// Set to 5x the announcement interval (2 s) to tolerate a few missed broadcasts.
#define PEER_TIMEOUT_MS 10000

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

  // Capture our own MAC for routing header population
  SelfMAC = GetMACAddress();
  Serial.print("Node MAC: ");
  Serial.println(SelfMAC.to_cstr());

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
// Also prunes peers that have not been heard from within PEER_TIMEOUT_MS.
void AnnouceMAC(){
  PruneStale();

  Message message = {};
  message.type = MessageType::Discovery;
  message.id = 0;
  memcpy(message.src, SelfMAC.GetAddressArray(), 6);
  memcpy(message.dst, BroadcastMAC.GetAddressArray(), 6);
  message.ttl = 1;

  bool success = SendMessage(BroadcastPeer, message);
  if (!success){
    Serial.println("ERR: Failed to send discovery message");
  } else {
    Serial.print("Announced presence to mesh w/ name: ");
    Serial.println(deviceUsername);
  }
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
    SendPeerList(source_peer); // Share known peers so sender can build routing table
  }

  // Refresh lastSeen so PruneStale doesn't evict this peer.
  auto existing = std::find_if(Peers.begin(), Peers.end(), [&](Peer& p){
    return p.GetMAC() == source;
  });
  if (existing != Peers.end()) existing->UpdateLastSeen();

  //Send reply
  if (message.id == 0){
    Message reply = {};
    reply.type = MessageType::DiscoveryResponse;
    reply.id = 1;
    memcpy(reply.src, SelfMAC.GetAddressArray(), 6);
    memcpy(reply.dst, source.GetAddressArray(), 6);
    reply.ttl = 1;
    bool success = SendMessage(source_peer, reply);
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
    SendPeerList(source_peer); // Share known peers so responder can build routing table
  }

  // Refresh lastSeen so PruneStale doesn't evict this peer.
  auto existing = std::find_if(Peers.begin(), Peers.end(), [&](Peer& p){
    return p.GetMAC() == source;
  });
  if (existing != Peers.end()) existing->UpdateLastSeen();
}

// HandleACK:
// Handle incoming ACK messages: clear waitingForAck when ACK matches expected ackId.
void HandleACK(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
#ifdef DEBUG
  Serial.print("DBG: Recieved ACK Message from: ");
  Serial.println(source.to_cstr());
#endif

  if (!waitingForAck) return; // Spurious ACK from an intermediate forwarding hop; ignore.

  if (message.id == ackId) {
    waitingForAck = false;
  } else {
    Serial.println("ERR: ACK id did not match message");
  }
}

// HandleText:
// Handle incoming Text messages.
// If the message's destination is not this node, forward it toward its destination via the
// routing table (decrementing TTL). Otherwise deliver locally and ACK the immediate sender.
void HandleText(const esp_now_recv_info* info, const Message message) {
  MAC sender = GetSenderMAC(info);
  MAC dst(std::vector<uint8_t>(message.dst, message.dst + 6));
  MAC src(std::vector<uint8_t>(message.src, message.src + 6));

  // ACK the immediate sender regardless of whether we're the final destination.
  // This lets each hop's retry loop clear without waiting for end-to-end delivery.
  auto sender_peer = FindPeer(sender);
  if (sender_peer.has_value()) {
    Message ack = {};
    ack.type = MessageType::ACK;
    ack.id = message.id;
    memcpy(ack.src, SelfMAC.GetAddressArray(), 6);
    memcpy(ack.dst, sender.GetAddressArray(), 6);
    ack.ttl = 1;
    SendMessage(*sender_peer, ack);
  } else {
    Serial.println("ERR: Sender peer not in peer list.");
  }

  // Forward if we are not the final destination.
  if (dst != SelfMAC && dst != BroadcastMAC) {
    if (message.ttl == 0) {
      Serial.println("ERR: TTL expired, dropping message");
      return;
    }
    auto nextHop = FindNextHop(dst);
    if (!nextHop.has_value()) {
      Serial.print("ERR: No route to ");
      Serial.println(dst.to_cstr());
      return;
    }
    Message fwd = message;
    fwd.ttl--;
    SendMessage(*nextHop, fwd);
#ifdef DEBUG
    Serial.print("DBG: Forwarded to ");
    Serial.println(nextHop->GetMAC().to_cstr());
#endif
    return;
  }

  // Deliver locally.
  if (isPager){
    Serial.println("receiving message...");
    size_t len = strnlen(message.info, MessageSize);
    std::string text(message.info, len);
    std::string sourceStr = src.to_string();
    std::string ret = 'm' + sourceStr + text;

    Serial.print("Forwarding to app: ");
    Serial.println(ret.c_str());
    SendToApp(std::as_bytes(std::span<char>(reinterpret_cast<char *>(ret.data()), ret.size())));
  } else {
    Serial.print("MSG from ");
    Serial.print(src.to_cstr());
    Serial.print(": ");
    Serial.println(message.info);
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
      Serial.print("Message Recieved: ");
      Serial.println(message.to_cstr());
      HandleText(info, message);
      break;
    case MessageType::PeerList:
      HandlePeerList(info, message);
      break;
    case MessageType::Invalid:
    default:
      Serial.print("Invalid Message Recieved: ");
      Serial.println(message.to_cstr());
      break;
  }
}

/*
// not sure what this is meant to do, not sure if we should do this either since we should send to
// server (and it handles sending to unconnected peers) not all peers
void SendToAllPeers(String msg) {
  auto it = std::find_if(Peers.begin(), Peers.end(), [&](const Peer& p) {
    SendTextMessage(p.GetMAC(), msg);
    return p.GetMAC() == source;
  });
}
 */

// PruneStale:
// Remove peers that have not sent a discovery heartbeat within PEER_TIMEOUT_MS.
// Also evicts any routing table entries whose next-hop was through the removed peer.
void PruneStale() {
  auto it = Peers.begin();
  while (it != Peers.end()) {
    if (it->IsStale(PEER_TIMEOUT_MS)) {
      MAC staleMac = it->GetMAC();
      Serial.print("Removing stale peer: ");
      Serial.println(staleMac.to_cstr());

      esp_now_del_peer(staleMac.GetAddressArray());

      // Remove routing table entries that used this peer as their next hop.
      RoutingTable.erase(
        std::remove_if(RoutingTable.begin(), RoutingTable.end(),
          [&staleMac](const std::pair<MAC, MAC>& r){ return r.second == staleMac; }),
        RoutingTable.end()
      );

      it = Peers.erase(it);
    } else {
      ++it;
    }
  }
}

// FindNextHop:
// Return the Peer to send to in order to reach dst.
// Checks direct peers first; falls back to RoutingTable for multi-hop paths.
std::optional<Peer> FindNextHop(MAC dst) {
  auto direct = FindPeer(dst);
  if (direct.has_value()) return direct;

  for (auto& route : RoutingTable) {
    if (route.first == dst) {
      return FindPeer(route.second);
    }
  }

  return std::nullopt;
}

// SendPeerList:
// Serialize this node's direct peer list into a PeerList message and send it to target.
// The receiver uses this to populate its routing table for multi-hop delivery.
void SendPeerList(Peer target) {
  if (Peers.empty()) return;

  Message msg = {};
  msg.type = MessageType::PeerList;
  msg.id = 0;
  memcpy(msg.src, SelfMAC.GetAddressArray(), 6);
  memcpy(msg.dst, target.GetMAC().GetAddressArray(), 6);
  msg.ttl = 1; // Peer list is direct-neighbor information only

  uint8_t count = (uint8_t)std::min(Peers.size(), (size_t)((MessageSize - 1) / 6));
  msg.info[0] = (char)count;
  for (uint8_t i = 0; i < count; i++) {
    memcpy(msg.info + 1 + i * 6, Peers[i].GetMAC().GetAddressArray(), 6);
  }

  bool success = SendMessage(target, msg);
  Serial.println(success ? "Peer list sent" : "ERR: Failed to send peer list");
}

// HandlePeerList:
// Parse a PeerList message from a direct neighbor and add entries to the routing table.
// For each MAC in the list that is not already directly reachable, record the sender as
// the next hop, enabling multi-hop delivery through that neighbor.
void HandlePeerList(const esp_now_recv_info* info, const Message message) {
  MAC sender = GetSenderMAC(info);
#ifdef DEBUG
  Serial.print("DBG: Received PeerList from: ");
  Serial.println(sender.to_cstr());
#endif

  uint8_t count = (uint8_t)message.info[0];
  for (uint8_t i = 0; i < count; i++) {
    std::vector<uint8_t> macBytes(
      (const uint8_t*)message.info + 1 + i * 6,
      (const uint8_t*)message.info + 1 + i * 6 + 6
    );
    MAC peerMAC(macBytes);

    if (peerMAC == SelfMAC) continue;
    if (FindPeer(peerMAC).has_value()) continue; // Already directly reachable

    // Check if a route to this MAC is already known
    bool routeExists = false;
    for (auto& route : RoutingTable) {
      if (route.first == peerMAC) { routeExists = true; break; }
    }

    if (!routeExists) {
      RoutingTable.emplace_back(peerMAC, sender);
      Serial.print("Route: ");
      Serial.print(peerMAC.to_cstr());
      Serial.print(" via ");
      Serial.println(sender.to_cstr());
    }
  }
}

// SendTextMessage:
// Split a long text string into MessageSize chunks and send each chunk as a Text message using
// SendMessageWithRetry. Resolves the next-hop peer via direct peer list or routing table.
// Falls back to broadcast when no route is known.
bool SendTextMessage(MAC receiver, String msg) {
  auto nextHop = FindNextHop(receiver);

  if (!nextHop.has_value()) {
    Serial.print("No route to ");
    Serial.println(receiver.to_cstr());
    Serial.println("Broadcasting...");
  }

  Message message = {};
  message.type = MessageType::Text;
  memcpy(message.src, SelfMAC.GetAddressArray(), 6);
  memcpy(message.dst, receiver.GetAddressArray(), 6);
  message.ttl = 5; // Allow up to 5 hops
  bool success = true;
  printf("Sending text message: %s\n", msg.c_str());

  if (msg.length() > MessageSize) {
    Serial.println("ERR: Cannot send message longer than MessageSize");
    return false;
  }

  strncpy(message.info, msg.c_str(), MessageSize);
#ifdef DEBUG
  Serial.println("DBG: Sending message");
#endif
  success = SendMessageWithRetry(nextHop.value_or(BroadcastPeer), message) && success;

  if (!success) {
    Serial.println("ERR: Failed to send text message.");
  }
  return success;
}

#endif
