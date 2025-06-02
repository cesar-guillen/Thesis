#include <stdio.h>
#include <ASCON.h>
#include <string.h>
#include "mbedtls/sha256.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"


void hash_file(fs::FS &fs, const char *path, unsigned char *finalhash) {
  Serial.printf("Hashing file contents of %s ... \n", path);
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  mbedtls_sha256_context sha_ctx;
  mbedtls_sha256_init(&sha_ctx);
  mbedtls_sha256_starts(&sha_ctx, 0);  // 0 = SHA-256, 1 = SHA-224

  size_t file_size = file.size();
  size_t file_hashed = 0;
  unsigned long start_time = millis();

  while (file_hashed < file_size) {
    size_t size_to_read = ((file_size - file_hashed) > CHUNK_SIZE ? CHUNK_SIZE : (file_size - file_hashed));
    char *curr_chunk = (char *)malloc(size_to_read);
    if (curr_chunk == nullptr) {
      Serial.println("Memory allocation failed");
      mbedtls_sha256_free(&sha_ctx);
      file.close();
      return;
    }

    file.readBytes(curr_chunk, size_to_read);
    mbedtls_sha256_update(&sha_ctx, (const unsigned char *)curr_chunk, size_to_read);
    file_hashed += size_to_read;
    free(curr_chunk);
  }

  mbedtls_sha256_finish(&sha_ctx, finalhash);
  mbedtls_sha256_free(&sha_ctx);
  file.close();

  unsigned long end_time = millis();
  float total_time = ((float)(end_time - start_time)) / 1000.0;
  Serial.printf("Hashing complete in %.4f seconds\n", total_time);
}


int compare_hashes(const unsigned char *first, const unsigned char *second) {
  return memcmp(first, second, HASH_SIZE) == 0;
}

void print_hash_output(unsigned long time, const unsigned char *finalhash) {
  Serial.printf("Hashing process completed in %lu milliseconds\n", time);
  Serial.print("SHA-256 Output: ");
  for (int i = 0; i < HASH_SIZE; i++) {
    Serial.printf("%02X", finalhash[i]);  
  }
  Serial.println();
}
