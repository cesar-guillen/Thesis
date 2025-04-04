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

int encrypt_chunk(char* plaintext, char* cyphertext, unsigned long long *clen){
  unsigned char *m = (unsigned char*)plaintext;
  unsigned long long mlen = strlen((char*)m);

  unsigned char ad[] = "";
  unsigned long long adlen = strlen((char*)ad);

  unsigned char npub[CRYPTO_NPUBBYTES] = {0}; //nonce
  unsigned char k[CRYPTO_KEYBYTES] = {0};     //key

  int enc_success = crypto_aead_encrypt((unsigned char*)cyphertext, clen, m, mlen, ad, adlen, NULL, npub, k);
  return enc_success;
}
/*
int decrypt_chunk(){
  unsigned char decrypted_m[mlen + 1]; // decrypted plaintext buffer
  unsigned long long decrypted_mlen;
  //int decrypt_status = crypto_aead_decrypt(decrypted_m, &decrypted_mlen, NULL, c, clen, ad, adlen, npub, k);

  if (decrypt_status == 0) {
      decrypted_m[decrypted_mlen] = '\0';
      Serial.printf("Decrypted output: %s\n", decrypted_m);
  } else {
      Serial.println("Decryption failed!");
  }
  return 0;
}
*/
void print_hex(const unsigned char* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char buf[3];
    sprintf(buf, "%02X", data[i]);
    Serial.printf("%s ",buf);
  }
  Serial.println();
}


void encrypt_file(fs::FS &fs, const char *path){
  Serial.printf("Encrypting file contents of %s ... \n", path);
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }
  size_t file_size = file.size();
  size_t file_encrypted = 0;
  unsigned long start_time = millis();
  while (file_encrypted < file_size)
  {
    size_t size_to_read = ((file_size - file_encrypted) > CHUNK_SIZE ? CHUNK_SIZE : (file_size - file_encrypted));
    char *curr_chunk = (char *)malloc(size_to_read);
    char *encrypted_chunk = (char *)malloc(size_to_read * 2 + CRYPTO_ABYTES);
    if ((curr_chunk == nullptr || encrypted_chunk == nullptr)) {
      Serial.println("Memory allocation failed");
      file.close();
      return;
    }
    file.readBytes(curr_chunk, size_to_read);
    unsigned long long clen;
    if (encrypt_chunk(curr_chunk, encrypted_chunk, &clen) < 0){
      Serial.printf("Encryption failed exiting...\n");
      free(curr_chunk);
      free(encrypted_chunk);
      file.close();
      return;
    }
    //print_hex((unsigned char*)encrypted_chunk, clen);
    file_encrypted += size_to_read;
    free(curr_chunk);
    free(encrypted_chunk);
  }
  unsigned long end_time = millis();
  unsigned long total_time = end_time - start_time;
  Serial.printf("Encryption process completed in %lu seconds\n", (double)total_time / 1000.0);
  file.close();
  return;
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
  encrypt_file(SD, "/zip.zip");
}

void loop() {

}
