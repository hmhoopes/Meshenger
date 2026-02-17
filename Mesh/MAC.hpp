#ifndef MESH_MAC_HPP
#define MESH_MAC_HPP

// WiFi & ESP Headers
#include <esp_now.h>
#include <esp_wifi.h>

// Utility Headers
#include <vector>
#include <string>
#include <assert.h>

class MAC {
  public:
    MAC() {}
    MAC(std::vector<uint8_t> aAddr) {
      assert(aAddr.size() == 6);
      memcpy(addr.data(), aAddr.data(), sizeof(uint8_t) * 6);
    }

    uint8_t* GetAddressArray() {return addr.data();}

    std::string to_string(){
      char buf[100];
      sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
                    addr[0], addr[1], addr[2],
                    addr[3], addr[4], addr[5]);
      return std::string(buf);
    }

    const char* to_cstr(){
      return to_string().c_str();
    }

  private:
    friend bool operator==(const MAC& left, const MAC& right);
    //Using vector to simplify moving data
    std::vector<uint8_t> addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

// Compare MAC addresses
bool operator==(const MAC& left, const MAC& right){
  return left.addr == right.addr;
}

// ╔═══════════════╗
// ║  MAC Helpers  ║
// ╚═══════════════╝

MAC GetMACAddress(){
  MAC baseMac;
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac.GetAddressArray());
  switch (ret){
    case ESP_ERR_INVALID_ARG:
      Serial.println("Error: ESP_ERR_INVALID_ARG");
    case ESP_ERR_WIFI_NOT_INIT:
      Serial.println("Error: ESP_ERR_WIFI_NOT_INIT (initialize wifi module in setup to fix)");
    case ESP_ERR_WIFI_IF:
      Serial.println("Error: ESP_ERR_WIFI_IF");
  }
  return std::move(baseMac);
}

MAC GetSenderMAC(const esp_now_recv_info* info){
  std::vector<uint8_t> srcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(srcAddr.data(), info->src_addr, 6);
  MAC src = MAC(srcAddr);
  return src;
}

MAC GetDestinationMAC(const esp_now_recv_info* info){
  std::vector<uint8_t> dstAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(dstAddr.data(), info->des_addr, 6);
  MAC dst = MAC(dstAddr);
  return dst;
}

#endif