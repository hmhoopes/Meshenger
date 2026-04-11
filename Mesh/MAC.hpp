/*
Project: Meshenger
Module Name: MAC.hpp
Description:
    Lightweight wrapper around a 6‑byte MAC address with formatting and
    helper functions for retrieving local and packet MACs.
Inputs:
    - Vectors of bytes or esp_now_recv_info pointers.
Outputs:
    - Formatted MAC strings and MAC objects.
External Sources:
    - esp_wifi, esp_now libraries
Author: Team 2
Creation Date: 02/17/2026
*/

#ifndef MESH_MAC_HPP
#define MESH_MAC_HPP

// WiFi & ESP Headers
#include <esp_now.h>
#include <esp_wifi.h>

// Utility Headers
#include <vector>
#include <string>
#include <assert.h>

// MAC:
// Lightweight wrapper around a 6-byte MAC address with helpers for formatting and access.
class MAC {
  public:
    MAC() {}
    MAC(std::vector<uint8_t> aAddr) {
      assert(aAddr.size() == 6);
      memcpy(addr.data(), aAddr.data(), sizeof(uint8_t) * 6);
    }

    // GetAddressArray:
    // Return raw pointer to the internal 6-byte address array for use with ESP-NOW APIs.
    uint8_t* GetAddressArray() {return addr.data();}
    const uint8_t* GetAddressArray() const {return addr.data();}

    // to_arduinostr:
    // Format MAC as an Arduino String (hex segments separated by ':') for BLE naming/logging.
    String to_arduinostr() const{
      String ret;
      ret = String(addr[0], HEX);
      ret = ret + ":";
      ret = ret + String(addr[1], HEX);
      ret = ret + ":";
      ret = ret + String(addr[2], HEX);
      ret = ret + ":";
      ret = ret + String(addr[3], HEX);
      ret = ret + ":";
      ret = ret + String(addr[4], HEX);
      ret = ret + ":";
      ret = ret + String(addr[5], HEX);
      return ret;
    }

    // to_string:
    // Return a std::string representation of the MAC in hex separated by ':'.
    std::string to_string() const{
      char buf[100];
      sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
                    addr[0], addr[1], addr[2],
                    addr[3], addr[4], addr[5]);
      return std::string(buf);
    }

    // to_cstr:
    // Return a C-style string pointer for quick logging (note: returns pointer into a temporary).
    const char* to_cstr() const{
      return to_string().c_str();
    }

  private:
    friend bool operator==(const MAC& left, const MAC& right);
    //Using vector to simplify moving data
    std::vector<uint8_t> addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

// operator==:
// Compare two MAC objects for equality by comparing their byte arrays.
bool operator==(const MAC& left, const MAC& right){
  return left.addr == right.addr;
}

// GetMACAddress:
// Retrieve this device's station MAC address via esp_wifi_get_mac.
MAC GetMACAddress(){
  MAC baseMac;
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac.GetAddressArray());
#ifdef SERIAL_LOG_DEBUG
  switch (ret){
    case ESP_ERR_INVALID_ARG:
      Serial.println("Error: ESP_ERR_INVALID_ARG");
    case ESP_ERR_WIFI_NOT_INIT:
      Serial.println("Error: ESP_ERR_WIFI_NOT_INIT (initialize wifi module in setup to fix)");
    case ESP_ERR_WIFI_IF:
      Serial.println("Error: ESP_ERR_WIFI_IF");
  }
#endif
  return std::move(baseMac);
}

// GetSenderMAC:
// Construct a MAC object from the source address in esp_now_recv_info.
MAC GetSenderMAC(const esp_now_recv_info* info){
  std::vector<uint8_t> srcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(srcAddr.data(), info->src_addr, 6);
  MAC src = MAC(srcAddr);
  return src;
}

// GetDestinationMAC:
// Construct a MAC object from the destination address in esp_now_recv_info.
MAC GetDestinationMAC(const esp_now_recv_info* info){
  std::vector<uint8_t> dstAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(dstAddr.data(), info->des_addr, 6);
  MAC dst = MAC(dstAddr);
  return dst;
}

#endif
