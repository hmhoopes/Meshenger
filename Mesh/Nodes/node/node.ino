/*
Project: Meshenger
Module Name: node.ino
Description:
    Basic node behaviour: announce MAC periodically and allow
    sending text messages over the mesh via serial. Supports multi-hop
    delivery — messages destined for nodes not directly reachable are
    forwarded through intermediate peers using the routing table.
Inputs:
    - Serial input lines.
Outputs:
    - ESP-NOW discovery broadcasts, peer list exchanges, and text messages.
External Sources:
    - Helpers.hpp
Author: Team 2
Creation Date: 02/11/2026
*/

#include "../../Helpers.hpp"

// setup:
// Initialize ESP-NOW for a node device.
void setup() {
  InitializeESPNow();
}

// loop:
// Periodically announce presence and allow sending a text message over the mesh.
// Displays both directly connected peers and multi-hop routable nodes each cycle.
// Sends to the first directly connected peer, or falls back to the first routable destination.
void loop() {
  AnnouceMAC();
#ifdef DEBUG
  Serial.println("DBG: Annouced");
#endif
  delay(2000);

  if (Peers.size() > 0) {
    Serial.print("Direct peers (");
    Serial.print(Peers.size());
    Serial.println("):");
    for (auto& p : Peers) {
      Serial.print("  ");
      Serial.println(p.GetMAC().to_cstr());
    }
  }

  if (RoutingTable.size() > 0) {
    Serial.println("Routable nodes:");
    for (auto& route : RoutingTable) {
      Serial.print("  ");
      Serial.print(route.first.to_cstr());
      Serial.print(" via ");
      Serial.println(route.second.to_cstr());
    }
  }

  // Prefer sending to a direct peer; fall back to first routable destination.
  std::optional<MAC> targetMAC;
  if (Peers.size() > 0) {
    targetMAC = Peers.front().GetMAC();
  } else if (RoutingTable.size() > 0) {
    targetMAC = RoutingTable.front().first;
  }

  if (targetMAC.has_value()) {
#ifdef DEBUG
    Serial.print("DBG: Target: ");
    Serial.println(targetMAC->to_cstr());
    Serial.println("DBG: Waiting for message.");
#endif
    auto inputMessage = Serial.readStringUntil('\n');
    inputMessage.trim();
    if (inputMessage.length() > 0) {
      SendTextMessage(*targetMAC, inputMessage);
    }
  }

  delay(2000);
}
