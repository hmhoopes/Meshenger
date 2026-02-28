#ifndef MESH_MESSAGE_HPP
#define MESH_MESSAGE_HPP

#include "MAC.hpp"
#include "Peer.hpp"

// WiFi & ESP Headers
#include <esp_now.h>

#include <optional>

typedef enum MessageType {
  Discovery,
  DiscoveryResponse,
  Text,
  ACK,
  NACK,
  Invalid,
} MessageType;

static constexpr int MessageSize = ESP_NOW_MAX_DATA_LEN - (sizeof(int) + sizeof(MessageType));

typedef struct Message {
    std::string to_string(){
      char buf[ESP_NOW_MAX_DATA_LEN];
      sprintf(buf, "Type: %d | ID: %d | Message: %s\n", type, id, info);
      return std::string(buf);
    }

    const char* to_cstr(){
      return to_string().c_str();
    }

    MessageType type;
    int id;
    char info[MessageSize];
} Message;

#define TIMEOUT 500 // Timeout in ms
#define MAX_RETRY 5 // Max number of retries

// Global message ACKs
bool waitingForAck = false;
int ackId = -1;

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
