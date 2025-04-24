#include <wolfssl/wolfcrypt/sha256.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#define CHUNK_SIZE 2048

void hash_file(fs::FS &fs, const char *path, unsigned char *finalhash) {
  Serial.printf("Hashing file contents of %s ... \n", path);
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  wc_Sha256 sha_ctx;
  wc_InitSha256(&sha_ctx);

  size_t file_size = file.size();
  size_t file_hashed = 0;
  unsigned long start_time = millis();

  while (file_hashed < file_size) {
    size_t size_to_read = ((file_size - file_hashed) > CHUNK_SIZE ? CHUNK_SIZE : (file_size - file_hashed));
    char *curr_chunk = (char *)malloc(size_to_read);
    if (curr_chunk == nullptr) {
      Serial.println("Memory allocation failed");
      wc_Sha256Free(&sha_ctx);
      file.close();
      return;
    }

    file.readBytes(curr_chunk, size_to_read);
    wc_Sha256Update(&sha_ctx, (const byte *)curr_chunk, size_to_read);
    file_hashed += size_to_read;
    free(curr_chunk);
  }

  wc_Sha256Final(&sha_ctx, finalhash);
  wc_Sha256Free(&sha_ctx);
  file.close();

  unsigned long end_time = millis();
  float total_time = ((float)(end_time - start_time)) / 1000.0;
  Serial.printf("Hashing complete in %.3f seconds\n", total_time);
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
