//WiFi Headers
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>

//Utility Headers
#include <vector>
#include <string>
#include <assert.h>

#define DEBUG 0

//================================== Forward Decls ===============================================
class MAC;
struct Message;
class Peer;
bool SendMessage(Peer target, const Message message);
//================================================================================================

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
      prev_added =  ret == ESP_ERR_ESPNOW_EXIST;
      Serial.println((prev_added) ? "peer already present" : "");
      return added || prev_added;
    }

    bool PrevAdded(){
      return prev_added;
    }

    bool IsAdded(){
      return added || prev_added;
    }

  private:
    friend bool SendMessage(Peer target, const Message message);

    bool added{false};
    bool prev_added{false};
    MAC mac;
    //TODO: should probably make this into a shared_ptr
    std::shared_ptr<esp_now_peer_info_t> peer_info = std::make_shared<esp_now_peer_info_t>();
};

MAC BroadcastMAC = MAC(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
Peer BroadcastPeer = Peer(BroadcastMAC);

std::vector<Peer> Peers;

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

  if (!BroadcastPeer.AddPeer()){
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
  DiscoveryResponse,
  Text,
  ACK,
  NACK,
  Invalid,
} MessageType;

static constexpr int MessageSize = ESP_NOW_MAX_DATA_LEN - (sizeof(int) + sizeof(MessageType));
typedef struct Message {
    MessageType type;
    int id;
    char info[MessageSize];
} Message;

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

bool SendMessage(Peer target, const Message message){
  // Send message via ESP-NOW
  if (!target.IsAdded()){
    Serial.print("Cannot send message to unadded peer target:");
    Serial.println(target.mac.to_cstr());
    return false;
  }
  esp_err_t result = esp_now_send(target.mac.GetAddressArray(), (uint8_t *) &message, sizeof(message));
   
  return result == ESP_OK;
}

void HandleDiscovery(const esp_now_recv_info* info, const Message message) {
  MAC source = GetSenderMAC(info);
  Serial.print("Recieved Discovery Message from: ");
  Serial.println(source.to_cstr());


  //Add peer
  Peer source_peer = Peer(source);
  if (!source_peer.AddPeer()){
    Serial.println("failed to add discovery sender as peer");
    return;
  }
  if (!source_peer.PrevAdded()){
    Peers.emplace_back(source_peer);
  }

  if (message.id == 0){
    //Send reply
    Message message;
    message.info[0] = '\0';
    message.type = MessageType::Discovery;
    message.id = 1;
    bool success = SendMessage(source_peer, message);
    Serial.println((success) ? "Replied successfully" : "Error: couldn't send message");
  }
}

void OnDataReceive(const esp_now_recv_info* info, const uint8_t *incomingData, int len){
  if (len != sizeof(Message)){
#if DEBUG
    Serial.println("Error: received message of different size than expected");
#endif
    return;
  }

  Message message;
  memcpy(&message, incomingData, sizeof(Message));

  switch (message.type){
    case MessageType::Discovery:
      HandleDiscovery(info, message);
      break;
    case MessageType::Invalid:
    default:
      Serial.print("Message Info: ");
      Serial.println(message.info);
      Serial.print("Message Type: ");
      Serial.println(message.type);
      Serial.print("Message ID: ");
      Serial.println(message.id);
      break;
  }
}

void RegisterListen() {esp_now_register_recv_cb(OnDataReceive);}

void AnnouceMAC(){
  Message message;
  message.info[0] = '\0';
  message.type = MessageType::Discovery;
  message.id = 0;
  
  bool success = SendMessage(BroadcastPeer, message);
  Serial.println((success) ? "Annouced successfully" : "Error: couldn't send message");
}