#include <stdio.h>
#include <string.h>
#include "../crypto/esp32/ascon.h"
#include "../crypto/esp32/hash.h"
#include "../crypto/esp32/core.h"
#include "../crypto/esp32/permutations.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define MAX_INPUT_LENGTH 100  
#define CRYPTO_BYTES 64   // Size of hash
#define CHUNK_SIZE 2048

char userInput[MAX_INPUT_LENGTH];  
int buffer_index = 0;  

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);
  
    File root = fs.open(dirname);
    if (!root) {
      Serial.println("Failed to open directory");
      return;
    }
    if (!root.isDirectory()) {
      Serial.println("Not a directory");
      return;
    }
  
    File file = root.openNextFile();
    while (file) {
      if (file.isDirectory()) {
        Serial.print("  DIR : ");
        Serial.println(file.name());
        if (levels) {
          listDir(fs, file.path(), levels - 1);
        }
      } else {
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("  SIZE: ");
        Serial.println(file.size());
      }
      file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);
  
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  hash_file(file);
  file.close();
}
  
void hash_file(fs::File file) {
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }
  file.seek(0);
  char unsigned chunkhash[CRYPTO_BYTES];
  char unsigned combinedhash[CRYPTO_BYTES*2];
  char unsigned finalhash[CRYPTO_BYTES];
  memset(chunkhash, 0, CRYPTO_BYTES);
  memset(combinedhash, 0, CRYPTO_BYTES*2);
  memset(finalhash, 0, CRYPTO_BYTES);
  size_t file_size = file.size();
  size_t file_hashed = 0;
  size_t loop = 0;
  unsigned long start_time = millis();
  while (file_hashed < file_size)
  {
    size_t size_to_read = ((file_size - file_hashed) > CHUNK_SIZE ? CHUNK_SIZE : (file_size - file_hashed));
    char *curr_chunk = (char *)malloc(size_to_read);
    if (curr_chunk == nullptr) {
      Serial.println("Memory allocation failed");
      file.close();
      return;
    }
    file.readBytes(curr_chunk, size_to_read);
    if (file_hashed == 0){
      hash_file_contents(finalhash,curr_chunk, CRYPTO_BYTES);
    }
    else{
      hash_file_contents(chunkhash, curr_chunk, CRYPTO_BYTES);
      memcpy(combinedhash, finalhash, CRYPTO_BYTES);
      memcpy(combinedhash + CRYPTO_BYTES, chunkhash, CRYPTO_BYTES);
      hash_file_contents(finalhash, (char *)combinedhash, CRYPTO_BYTES*2);
    }
    file_hashed += size_to_read;
    loop+=1;
    //Serial.printf("In iteration:%d , read: %d out of %d\n", loop, file_hashed, file_size);
    free(curr_chunk);
  }
  unsigned long end_time = millis();
  Serial.printf("Hashing process completed in %lu milliseconds\n", end_time - start_time);
  Serial.print("final output: ");
  for (int i = 0; i < CRYPTO_BYTES; i++) {
    Serial.printf("%02X", finalhash[i]);  
  }
  Serial.println();
  file.close();
}

void hash_file_contents(char unsigned* out, char * file_contents, size_t length){
  unsigned const char * input_to_hash = (unsigned const char *)file_contents;
  int test = crypto_hash(out, input_to_hash, length);
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
  uint8_t cardType = SD.cardType();
  
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  
  listDir(SD, "/", 0);
  readFile(SD, "/zip.zip");
}

void loop() {

}
