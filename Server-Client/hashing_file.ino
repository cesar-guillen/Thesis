#include <stdio.h>
#include <ASCON.h>
#include <string.h>
#include "../crypto/esp32/hash.h"
#include "../crypto/esp32/core.h"
#include "../crypto/esp32/permutations.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define CRYPTO_BYTES 64   // Size of hash
#define CHUNK_SIZE 2048

void hash_file(fs::FS &fs, const char *path, char unsigned *finalhash) {
  Serial.printf("Hashing file contents of %s ... \n", path);
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }
  char unsigned chunkhash[CRYPTO_BYTES] = { 0 };
  char unsigned combinedhash[CRYPTO_BYTES*2] = { 0 };
  size_t file_size = file.size();
  size_t file_hashed = 0;
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
      hash_file_contents(finalhash,curr_chunk, size_to_read);
    }
    else{
      hash_file_contents(chunkhash, curr_chunk, size_to_read);
      memcpy(combinedhash, finalhash, CRYPTO_BYTES);
      memcpy(combinedhash + CRYPTO_BYTES, chunkhash, CRYPTO_BYTES);
      hash_file_contents(finalhash, (char *)combinedhash, CRYPTO_BYTES*2);
    }
    file_hashed += size_to_read;
    free(curr_chunk);
  }
  unsigned long end_time = millis();
  unsigned long total_time = end_time - start_time;
  //print_hash_output(total_time, finalhash);
  file.close();
  return;
}

void print_hash_output(unsigned long time, char unsigned * finalhash){
  Serial.printf("Hashing process completed in %lu milliseconds\n", time);
  Serial.print("final output: ");
  for (int i = 0; i < CRYPTO_BYTES; i++) {
    Serial.printf("%02X", finalhash[i]);  
  }
  Serial.println();
}

void hash_file_contents(char unsigned* out, char * file_contents, size_t length){
  unsigned const char * input_to_hash = (unsigned const char *)file_contents;
  int test = crypto_hash(out, input_to_hash, length);
  return;
}




