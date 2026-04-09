/*
Project: Meshenger
Module Name: Message.hpp
Description:
    Defines the Message struct, MessageType enum, payload size constant,
    and send routines (with retry/ACK logic) used by the mesh.
Inputs:
    - targets, Message instances, MAC addresses.
Outputs:
    - Message formatting helpers, send/ retry functions.
External Sources:
    - esp_now library, STL string/optional.
Author: Team 2
Creation Date: 02/17/2026
*/

#ifndef MESH_MESSAGE_HPP
#define MESH_MESSAGE_HPP

#include "MAC.hpp"
#include "Helpers.hpp"

// WiFi & ESP Headers
#include <esp_now.h>

#include <optional>

// MessageType:
// Enumerates the different kinds of messages that can be sent/received in the mesh.
typedef enum MessageType {
  ReqTarget,
  ResTarget,
  Discovery,
  DiscoveryResponse,
  Text,
  ACK,
  NACK,
  PeerList,  // Share known peers for multi-hop routing
  Invalid,
} MessageType;

struct MessageHeader {
  MessageType type = MessageType::Invalid;
  int id = 0;
  unsigned char source[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  unsigned char target[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};
// MessageSize:
// Size of the payload buffer for a Message, computed from ESP-NOW max payload less header fields.
// Accounts for routing fields: src (6), dst (6), ttl (1).
static constexpr int MessageSize = ESP_NOW_MAX_DATA_LEN - (sizeof(int) + sizeof(MessageType) + 6 + 6 + sizeof(uint8_t));

// Message:
// Container for a mesh message including type, id, routing header, and a fixed-size payload buffer.
// src/dst carry the original source and final destination MACs for multi-hop routing.
// ttl limits the number of hops a message may traverse.
typedef struct Message {
    std::string to_string() const {
      char buf[300];
      sprintf(buf, "Type: %d | ID: %d | TTL: %d | Message: %s\n", type, id, ttl, info);
      return std::string(buf);
    }

    const char* to_cstr() const {
      return to_string().c_str();
    }

    MessageType type;
    int id;
    uint8_t src[6];   // Original source MAC
    uint8_t dst[6];   // Final destination MAC
    uint8_t ttl;      // Hops remaining; decremented on each forward
    char info[MessageSize];
};
#pragma pack(pop)

// TIMEOUT:
// Base timeout in milliseconds used for retransmit timing when waiting for ACKs.
#define TIMEOUT 500 // Timeout in ms
// MAX_RETRY:
// Maximum number of resend attempts before giving up on a message.
#define MAX_RETRY 5 // Max number of retries

// Global message ACK state:
// waitingForAck: whether sender is currently waiting for an ACK.
// ackId: id of the message currently awaiting ACK.
bool waitingForAck = false;
int ackId = -1;

// SendMessage:
// Sends a message to the target via ESP-NOW without higher-level ACK/retry handling.
// Returns true on esp_now_send success.
bool SendMessage(Peer target, const Message message) {
  if (!target.IsAdded()){
    Serial.print("ERR: Cannot send message to unadded peer target: ");
    Serial.println(target.mac.to_cstr());
    return false;
  }
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(target.mac.GetAddressArray(), (uint8_t *) &message, sizeof(message));

  // Only ACKs at MAC level
  return result == ESP_OK;
}

// SendMessageWithRetry:
// Sends a message and blocks (with retries) until an ACK is received or the retry limit is reached.
// Uses TIMEOUT and MAX_RETRY to control behavior.
bool SendMessageWithRetry(Peer target, const Message message) {
  if (!target.IsAdded()){
    Serial.print("ERR: Cannot send message to unadded peer target: ");
    Serial.println(target.mac.to_cstr());
    return false;
  }
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(target.mac.GetAddressArray(), (uint8_t *) &message, sizeof(message));

  waitingForAck = true;
  ackId = message.id;
  unsigned long sendTime = millis(); // Message send start send time
  int retryCount = 0; // Count the number of times we've retried

  // Block while messages aren't ACKed
  while (true) {
    delay(50);

    if (!waitingForAck) {
      return true;
    }

    if (millis() - sendTime > (TIMEOUT * (retryCount + 1))) {
      if (retryCount >= MAX_RETRY) {
        Serial.println("ERR: Message failed to send - retry threshold reached");
        return false;
      }
      retryCount++;
      Serial.printf("DBG: Resending message - attempt %d\n", retryCount);
      result = esp_now_send(target.mac.GetAddressArray(), (uint8_t *) &message, sizeof(message));
      sendTime = millis();
      waitingForAck = true;
    }
  }

  // Only ACKs at MAC level
  return result == ESP_OK;
}


#endif
