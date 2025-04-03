#include <stdio.h>
#include <string.h>
#include "../crypto/esp32/ascon.h"
#include "../crypto/esp32/hash.h"
#include "../crypto/esp32/permutations.h"
#include "../crypto/esp32/permutations.c"
#include "../crypto/printstate.h"
#include "../crypto/printstate.c"
#include "../crypto/esp32/core.h"
#include "../crypto/esp32/core.c"
#include "../crypto/esp32/encrypt.c"
#include "../crypto/esp32/decrypt.c"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#define CHUNK_SIZE 2048

void encryption_test(){
  unsigned char m[] = "testing encryption on simple string!";
  unsigned long long mlen = strlen((char*)m);

  unsigned char ad[] = "";
  unsigned long long adlen = strlen((char*)ad);

  unsigned char npub[CRYPTO_NPUBBYTES] = {0}; //nonce
  unsigned char k[CRYPTO_KEYBYTES] = {0};     //key

  unsigned char c[mlen + CRYPTO_ABYTES]; //ciphertext buffer
  unsigned long long clen;
  crypto_aead_encrypt(c, &clen, m, mlen, ad, adlen, npub, npub, k);
  Serial.printf("Encrypted output: ");

  unsigned char decrypted_m[mlen + 1]; // decrypted plaintext buffer
  unsigned long long decrypted_mlen;
  int decrypt_status = crypto_aead_decrypt(decrypted_m, &decrypted_mlen, NULL, c, clen, ad, adlen, npub, k);

  if (decrypt_status == 0) {
      decrypted_m[decrypted_mlen] = '\0';
      Serial.printf("Decrypted output: %s\n", decrypted_m);
  } else {
      Serial.println("Decryption failed!");
  }
}

void setup() {
  Serial.begin(115200);
  
#ifdef REASSIGN_PINS
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {
#else
  if (!SD.begin()) {
#endif
    Serial.println("Card Mount Failed");
    return;
  }
  listDir(SD, "/", 0);
  readFile(SD, "/test.txt");
  encryption_test();
}

void loop() {

}
