#ifndef CRYPTO_HPP
#define CRYPTO_HPP

//cpp headers
#include <vector>
#include <span>
#include <assert.h>

//crypto headers
#include <Crypto.h>
#include <SHA256.h>
#include <AES.h>
#include <P521.h>

#define P521_PUBKEY_SIZE 132
#define P521_SIG_SIZE P521_PUBKEY_SIZE
#define P521_PRIVKEY_SIZE 66
#define SHA256_SIZE 32
#define AES_KEY_SIZE SHA256_SIZE
#define AES_BLOCK_SIZE 16

uint8_t Root_PubKey[P521_PUBKEY_SIZE] = {
    0x00, 0x80, 0x71, 0x7C, 0x40, 0x15, 0x4A, 0x15, 0xCB, 0x46, 0xD3, 0x13, 0xD1, 
    0x40, 0x4A, 0xBE, 0x6D, 0xD4, 0xC1, 0xA1, 0x21, 0x5D, 0x41, 0x33, 0xDD, 0x7A, 
    0x16, 0x6A, 0x20, 0x8D, 0x23, 0x88, 0x4C, 0x13, 0x81, 0xA3, 0xFF, 0xD1, 0x91, 
    0x27, 0xF5, 0x5F, 0x3A, 0xCB, 0x3A, 0x0B, 0xBA, 0xDB, 0x9A, 0xBC, 0x4F, 0x61, 
    0xDB, 0x19, 0xC4, 0x96, 0x80, 0x5C, 0xE8, 0xCB, 0x70, 0x7B, 0xE2, 0x7A, 0x35, 
    0x6A, 0x00, 0x59, 0x0F, 0xE1, 0xC7, 0xCB, 0x8F, 0x55, 0xC5, 0xA6, 0x6F, 0x13, 
    0x08, 0x63, 0xE5, 0x86, 0xC4, 0x08, 0x3F, 0x97, 0x8B, 0x08, 0x6B, 0x50, 0x52, 
    0xB2, 0x3B, 0xDD, 0x9C, 0xE7, 0xAE, 0x03, 0xB1, 0x2D, 0x84, 0xB0, 0x86, 0xBB, 
    0x99, 0xAF, 0xBA, 0xED, 0xC7, 0xCA, 0xB6, 0xBE, 0xB2, 0x5A, 0xEC, 0xD2, 0x82, 
    0x1E, 0x13, 0xC6, 0x30, 0x11, 0xC7, 0xD4, 0x58, 0xF2, 0x00, 0xFE, 0x8B, 0xEA, 
    0x9A, 0x6C
};
// This shouldn't be something that is public, but left public for working on this as a school project. In the real-world, this wouldn't be public
uint8_t Root_PrivKey[P521_PRIVKEY_SIZE] = {
    0x00, 0x49, 0xEE, 0x55, 0xD2, 0x6D, 0x56, 0xFF, 0x5F, 0x4F, 0x78, 0xFA, 0x6E, 
    0xD8, 0x0A, 0xC8, 0x5D, 0x18, 0x2E, 0x6A, 0xBD, 0x2E, 0xCD, 0x78, 0x48, 0xE7, 
    0x1E, 0x87, 0xB1, 0x69, 0x76, 0x63, 0x63, 0xEB, 0xD7, 0xDB, 0x5E, 0x5B, 0xAE, 
    0xB3, 0xE7, 0xCF, 0x00, 0x57, 0x11, 0xC7, 0x35, 0x4E, 0x3A, 0x18, 0x1E, 0x73, 
    0x1D, 0x8D, 0xB2, 0xAF, 0xBF, 0x9A, 0x73, 0xFC, 0x32, 0xC7, 0x51, 0x24, 0x93, 
    0x71,
};

SHA256 sha256;

#define MESSAGE_LIM 100

//item to be kept in list, tied to a user
//created from P521 shared secret
class Tunnel {
  public:
    std::vector<uint8_t> EncryptMessage(std::span<const std::byte> aMessage){
      std::vector<uint8_t> cipher;
      const int blocks = std::ceil(static_cast<float>(aMessage.size()) / AES_BLOCK_SIZE);
      cipher.resize(AES_BLOCK_SIZE * blocks);
      for (int i = 0; i < blocks; i++){
        aes.encryptBlock(reinterpret_cast<uint8_t *>(cipher.data()) + i*AES_BLOCK_SIZE, reinterpret_cast<const uint8_t *>(aMessage.data()) + i*AES_BLOCK_SIZE);
      } 
      return cipher;
    } 

    std::vector<uint8_t> DecryptMessage(std::span<const std::byte> aCipherText){
      std::vector<uint8_t> plain;
      const int blocks = std::ceil(static_cast<float>(aCipherText.size()) / AES_BLOCK_SIZE);
      plain.resize(AES_BLOCK_SIZE * blocks);
      for (int i = 0; i < blocks; i++){
        aes.decryptBlock(reinterpret_cast<uint8_t *>(plain.data()) + i*AES_BLOCK_SIZE, reinterpret_cast<const uint8_t *>(aCipherText.data()) + i*AES_BLOCK_SIZE);
      } 
      return plain;
    }

    bool CheckValid(){
      return use_count <= MESSAGE_LIM;
    }

    Tunnel(std::span<const std::byte> aSharedSecret){
      assert(aSharedSecret.size() == P521_PRIVKEY_SIZE);

      secret_key.reserve(SHA256_SIZE);
      sha256.update(aSharedSecret.data(), P521_PRIVKEY_SIZE);
      sha256.finalize(secret_key.data(), SHA256_SIZE);
      sha256.clear();
      
      aes.setKey(secret_key.data(), AES_KEY_SIZE);
    }

  private:
    std::vector<uint8_t> secret_key;
    AES256 aes;
    int use_count = 0;
};

// function to setup connection, given user/target, return Tunnel
// TODO: setup this once messaging infrastructure is present
//  make it friend of Tunnel?

// function to verify a certificate, given pub key and signature
bool Verify(std::span<const std::byte> aSig, std::span<const std::byte> aMessage){
  assert(aSig.size() == P521_SIG_SIZE);
  return P521::verify(reinterpret_cast<const uint8_t *>(aSig.data()), Root_PubKey, aMessage.data(), aMessage.size());
}


#endif