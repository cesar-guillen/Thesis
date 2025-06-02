#include <stdio.h>
#include <ASCON.h>
#include <string.h>
#include "../crypto/esp32/hash.h"
#include "../crypto/esp32/core.h"
#include "../crypto/esp32/permutations.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#define CHUNK_SIZE 1024

void hash_file(fs::FS &fs, const char *path, unsigned char *finalhash) {
  Serial.printf("Hashing file contents of %s ... \n", path);
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  // Initialize ASCON hash context
  ascon_hash_state_t ctx;
  ascon_hash_init(&ctx);

  size_t file_size = file.size();
  size_t file_hashed = 0;

  unsigned long start_time = millis();

  // Allocate buffer once
  unsigned char *curr_chunk = (unsigned char *)malloc(CHUNK_SIZE);
  if (curr_chunk == nullptr) {
    Serial.println("Memory allocation failed");
    file.close();
    return;
  }

  while (file.available()) {
    size_t to_read = (CHUNK_SIZE < (file_size - file_hashed)) ? CHUNK_SIZE : (file_size - file_hashed);
    size_t actually_read = file.readBytes((char *)curr_chunk, to_read);
    if (actually_read > 0) {
      ascon_hash_update(&ctx, curr_chunk, actually_read);
      file_hashed += actually_read;
    } else {
      break; // read error or EOF
    }
  }

  ascon_hash_finalize(&ctx, finalhash);

  unsigned long end_time = millis();
  float total_time = ((float)(end_time - start_time)) / 1000.0;
  Serial.printf("Hashing complete in %.3f seconds\n", total_time);

  file.close();
  free(curr_chunk);
}


int compare_hashes(const unsigned char *first, const unsigned char *second) {
  return memcmp(first, second, CRYPTO_BYTES) == 0;
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




