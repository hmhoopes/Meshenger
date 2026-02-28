#ifndef MESH_PEER_HPP
#define MESH_PEER_HPP

#include "MAC.hpp"
#include "Message.hpp"

// WiFi & ESP Headers
#include <esp_now.h>

// Utility Headers
#include <vector>
#include <string>
#include <memory>

// Abstraction for interacting with other ESP32 devices
class Peer {
  public:
    Peer(MAC aMac) : mac{aMac} {
      memcpy(peer_info->peer_addr, mac.GetAddressArray(), 6);
      peer_info->channel = 0;  
      peer_info->encrypt = false;
    }

    //TODO: add method of interacting with source peer properties
    //TODO: investigate if we need to prevent changes after adding?
    const MAC GetMAC() const{
      return mac;
    }

    bool AddPeer(){
      auto ret = esp_now_add_peer(peer_info.get());
      added = ret == ESP_OK ;
      prev_added = ret == ESP_ERR_ESPNOW_EXIST;
      return added || prev_added;
    }

    bool PrevAdded(){
      return prev_added;
    }

    bool IsAdded(){
      return added || prev_added;
    }
    
    MAC mac;

  private:
    bool added{false};
    bool prev_added{false};
    std::shared_ptr<esp_now_peer_info_t> peer_info = std::make_shared<esp_now_peer_info_t>();
};

#endif
