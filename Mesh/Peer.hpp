/*
Project: Meshenger
Module Name: Peer.hpp
Description:
    Encapsulates ESP-NOW peer information and provides methods to
    add/check peers in the mesh network.
Inputs:
    - MAC address for the peer.
Outputs:
    - Peer addition status and MAC accessors.
External Sources:
    - esp_now library
Author: Team 2
Creation Date: 02/17/2026
*/


#ifndef MESH_PEER_HPP
#define MESH_PEER_HPP

#include "MAC.hpp"

// WiFi & ESP Headers
#include <esp_now.h>

// Utility Headers
#include <vector>
#include <string>
#include <memory>

// Peer:
// Abstraction for a remote mesh peer. Stores its MAC and esp_now_peer_info_t for adding to ESP-NOW.
class Peer {
  public:
    // Constructor:
    // Initialize peer_info from the provided MAC address.
    Peer(MAC aMac) : mac{aMac} {
      memcpy(peer_info->peer_addr, mac.GetAddressArray(), 6);
      peer_info->channel = 0;  
      peer_info->encrypt = false;
    }

    // GetMAC:
    // Return a copy of the Peer MAC address.
    const MAC GetMAC() const{
      return mac;
    }

    // AddPeer:
    // Add this peer to ESP-NOW peer list; sets added/prev_added flags accordingly.
    bool AddPeer(){
      auto ret = esp_now_add_peer(peer_info.get());
      added = ret == ESP_OK ;
      prev_added = ret == ESP_ERR_ESPNOW_EXIST;
      return added || prev_added;
    }

    // PrevAdded:
    // Return true if peer already existed in ESP-NOW when attempting to add.
    bool PrevAdded(){
      return prev_added;
    }

    // IsAdded:
    // Return true if the peer has been added (or was already present).
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