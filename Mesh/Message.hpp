#ifndef MESH_MESSAGE_HPP
#define MESH_MESSAGE_HPP

#include "MAC.hpp"
#include "Peer.hpp"

// WiFi & ESP Headers
#include <esp_now.h>

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

bool SendMessage(Peer target, const Message message){
  // Send message via ESP-NOW
  // TODO: Keep track of expected sequence numbers (message ids) within each peer
  if (!target.IsAdded()){
    Serial.print("Cannot send message to unadded peer target:");
    Serial.println(target.mac.to_cstr());
    return false;
  }
  esp_err_t result = esp_now_send(target.mac.GetAddressArray(), (uint8_t *) &message, sizeof(message));
   
  return result == ESP_OK;
}

#endif