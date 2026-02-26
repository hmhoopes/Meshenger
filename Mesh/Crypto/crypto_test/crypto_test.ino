//Cryptography header
#include "../../Crypto/Crypto.hpp"

#include <string>
#include <cmath>

// crypto headers
#include <Crypto.h>
#include <P521.h>
#include <SHA256.h>
#include <AES.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:
  //Demo for using symmetric encryption w/ shared secret
  uint8_t f1[P521_PRIVKEY_SIZE];
  uint8_t k1[P521_PUBKEY_SIZE];
  P521::dh1(k1, f1);

  uint8_t f2[P521_PRIVKEY_SIZE];
  uint8_t k2[P521_PUBKEY_SIZE];
  P521::dh1(k2, f2);

  P521::dh2(k1, f2);
  P521::dh2(k2, f1);

  sha256.update(reinterpret_cast<void *>(f1), P521_PRIVKEY_SIZE);

  uint8_t hash[SHA256_SIZE];
  sha256.finalize(reinterpret_cast<void *>(hash), SHA256_SIZE);
  sha256.clear();

  char hash_str[2*SHA256_SIZE + 1];
  for(int i = 0; i < SHA256_SIZE; i++){
    sprintf(hash_str + 2*i, "%02X", hash[i]);
  }
  hash_str[2*SHA256_SIZE] = 0;

  Serial.print("hash: 0x");
  Serial.println(std::string(hash_str).c_str());

  char message[AES_BLOCK_SIZE] = "Hello!";

  uint8_t cipher[AES_BLOCK_SIZE];
  AES256 aes;
  aes.setKey(hash, SHA256_SIZE);
  for (int i = 0; i < std::ceil(static_cast<float>(sizeof(message)) / AES_BLOCK_SIZE); i++){
    Serial.println(i);
    aes.encryptBlock(cipher + i*AES_BLOCK_SIZE, reinterpret_cast<uint8_t *>(message) + i*AES_BLOCK_SIZE);
  }

  aes.clear();
  aes.setKey(hash, SHA256_SIZE);
  char message_decrypt[AES_BLOCK_SIZE];
  message_decrypt[16] = '\0';
  for (int i = 0; i < std::ceil(static_cast<float>(sizeof(cipher)) / AES_BLOCK_SIZE); i++){
    Serial.println(i);
    aes.decryptBlock(reinterpret_cast<uint8_t *>(message_decrypt) + i*AES_BLOCK_SIZE, cipher + i*AES_BLOCK_SIZE);
  }

  Serial.print("message: ");
  Serial.println(std::string(message).c_str());

  Serial.print("message_decrypt: ");
  Serial.println(std::string(message_decrypt).c_str());

//Demo for generating shared secrets
/*   uint8_t f1[P521_PRIVKEY_SIZE];
  uint8_t k1[P521_PUBKEY_SIZE];
  P521::dh1(k1, f1);

  uint8_t f2[P521_PRIVKEY_SIZE];
  uint8_t k2[P521_PUBKEY_SIZE];
  P521::dh1(k2, f2);

  P521::dh2(k1, f2);
  P521::dh2(k2, f1);

  bool matches = true;
  for(int i = 0; i < P521_PRIVKEY_SIZE; i++){
    matches = matches && f1[i] == f2[i]; 
  }

  Serial.print("matches: ");
  Serial.println(matches ? "true" : "false"); */

//Demo for signing and verifying with root public key
/*   char message[] = {
    'H', 'e', 'l', 'l', 'o', '!'
  };
  uint8_t sig[P521_PUBKEY_SIZE];
  P521::sign(sig, Root_PrivKey, reinterpret_cast<void *>(message), sizeof(message));

  char sig_str[2*P521_PUBKEY_SIZE + 1];
  for(int i = 0; i < P521_PUBKEY_SIZE; i++){
    sprintf(sig_str + 2*i, "%02X", sig[i]);
  }
  sig_str[2*P521_PUBKEY_SIZE] = 0;

  bool verified = P521::verify(sig, Root_PubKey, reinterpret_cast<void *>(message), sizeof(message));

  Serial.print("sig: 0x");
  Serial.println(std::string(sig_str).c_str());
  Serial.print("verified: ");
  Serial.println(verified ? "true" : "false"); */


//Demo for generating ephermeral key
/*   uint8_t f[P521_PRIVKEY_SIZE];
  uint8_t k[P521_PUBKEY_SIZE];
  P521::dh1(k, f);

  char k_str[2*P521_PUBKEY_SIZE + 1];
  for(int i = 0; i < P521_PUBKEY_SIZE; i++){
    sprintf(k_str + 2*i, "%02X", k[i]);
  }
  k_str[2*P521_PUBKEY_SIZE] = 0;

  char f_str[2*P521_PRIVKEY_SIZE + 1];
  for(int i = 0; i < P521_PRIVKEY_SIZE; i++){
    sprintf(f_str + 2*i, "%02X", f[i]);
  }
  f_str[2*P521_PRIVKEY_SIZE] = 0;

  Serial.print("K: 0x");
  Serial.println(std::string(k_str).c_str());
  Serial.print("f: 0x");
  Serial.println(std::string(f_str).c_str());
  delay(4000); */

}