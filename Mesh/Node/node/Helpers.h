//WiFi Headers
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>

//Utility Headers
#include <vector>
#include <string>
#include <assert.h>

#define DEBUG 0

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

bool operator==(const MAC& left, const MAC& right){
  return left.addr == right.addr;
}

MAC BroadcastMAC = MAC(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
esp_now_peer_info_t BroadcastPeer;

void InitializeESPNow(){
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    assert(false);
  }

  //Init broadcast address
  memcpy(BroadcastPeer.peer_addr, BroadcastMAC.GetAddressArray(), 6);
  BroadcastPeer.channel = 0;  
  BroadcastPeer.encrypt = false;    //i think necessary for sending broadcast / enc isn't supported for broadcast
  if (esp_now_add_peer(&BroadcastPeer) != ESP_OK){
    Serial.println("Failed to add broadcast as peer");
    assert(false);
  }
}

MAC GetMACAddress(){
  MAC baseMac;
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac.GetAddressArray());
#if DEBUG
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

typedef enum MessageType {
  Discovery,
  Text,
  ACK,
  NACK,
  Invalid,
} MessageType;

typedef struct Message {
    MessageType type;
    int id;
    char info[ESP_NOW_MAX_DATA_LEN - (sizeof(int) + sizeof(MessageType))];
} Message;

void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len){
  if (len != sizeof(Message)){
#if DEBUG
    Serial.println("Error: received message of different size than expected");
#endif
    return;
  }

  std::vector<uint8_t> srcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(info->src_addr, srcAddr.data(), 6);
  MAC src = MAC(srcAddr);
  Serial.print("Message Sender: ");
  Serial.println(src.to_cstr());

  std::vector<uint8_t> dstAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(info->des_addr, dstAddr.data(), 6);
  MAC dst = MAC(dstAddr);
  Serial.print("Message Destination: ");
  Serial.println(dst.to_cstr());

  Message message;
  memcpy(&message, incomingData, sizeof(Message));
  Serial.print("Message Info: ");
  Serial.println(message.info);
  Serial.print("Message Type: ");
  Serial.println(message.type);
  Serial.print("Message ID: ");
  Serial.println(message.id);
}

void RegisterListen() {esp_now_register_recv_cb(OnDataReceive);}

void Broadcast(){
  Message message;
  // Set values to send
  strcpy(message.info, "THIS IS A CHAR");
  message.type = MessageType::Text;
  message.id = 1;
  
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(BroadcastMAC.GetAddressArray(), (uint8_t *) &message, sizeof(message));
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}