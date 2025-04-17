#include "mbedtls/aes.h"

#define CHUNK_SIZE 16
#define KEY_SIZE 32    
#define IV_SIZE 16
#define CRYPTO_BYTES 64


/* your 256-bit key */ 
const uint8_t key[KEY_SIZE] = { 0 };
const uint8_t iv[IV_SIZE] = { 0 };
unsigned char firsthash[CRYPTO_BYTES] = {0};
unsigned char secondhash[CRYPTO_BYTES] = {0};


void encrypt_file_2(fs::FS &fs, const char *inputPath, const char *outputPath) {
  File inFile = fs.open(inputPath, "r");
  File outFile = fs.open(outputPath, "w");

  if (!inFile || !outFile) {
    Serial.println("File open failed");
    return;
  }

  size_t originalSize = inFile.size(); // <-- get original size

  // Write size as header (4 bytes, little endian)
  outFile.write((uint8_t*)&originalSize, sizeof(originalSize));

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, KEY_SIZE * 8);

  uint8_t buffer[CHUNK_SIZE];
  uint8_t output[CHUNK_SIZE];
  uint8_t iv_copy[IV_SIZE];
  memcpy(iv_copy, iv, IV_SIZE);

  while (inFile.available()) {
    size_t readLen = inFile.read(buffer, CHUNK_SIZE);
    if (readLen < CHUNK_SIZE)
      memset(buffer + readLen, 0, CHUNK_SIZE - readLen); // zero-padding

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, CHUNK_SIZE, iv_copy, buffer, output);
    outFile.write(output, CHUNK_SIZE);
  }

  mbedtls_aes_free(&aes);
  inFile.close();
  outFile.close();
}


void decrypt_file_2(fs::FS &fs, const char *inputPath, const char *outputPath) {
  File inFile = fs.open(inputPath, "r");
  File outFile = fs.open(outputPath, "w");

  if (!inFile || !outFile) {
    Serial.println("File open failed");
    return;
  }

  // Read original size (first 4 bytes)
  size_t originalSize = 0;
  inFile.read((uint8_t*)&originalSize, sizeof(originalSize));

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, key, KEY_SIZE * 8);

  uint8_t buffer[CHUNK_SIZE];
  uint8_t output[CHUNK_SIZE];
  uint8_t iv_copy[IV_SIZE];
  memcpy(iv_copy, iv, IV_SIZE);

  size_t totalWritten = 0;
  while (inFile.available() && totalWritten < originalSize) {
    size_t readLen = inFile.read(buffer, CHUNK_SIZE);
    if (readLen != CHUNK_SIZE) break;

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, CHUNK_SIZE, iv_copy, buffer, output);

    size_t bytesToWrite = (originalSize - totalWritten) > CHUNK_SIZE ? CHUNK_SIZE : (originalSize - totalWritten);
    outFile.write(output, bytesToWrite);
    totalWritten += bytesToWrite;
  }

  mbedtls_aes_free(&aes);
  inFile.close();
  outFile.close();
}


void decrypt_verify(String file){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  deleteFile(SD, requested_file_name.c_str());
  Serial.println("Decrypting file ...");
  decrypt_file_2(SD, (requested_file_name + ".ascon").c_str(),requested_file_name.c_str());
  deleteFile(SD, (requested_file_name + ".ascon").c_str());
  char unsigned  hash_results[CRYPTO_BYTES] = { 0 };
  hash_file(SD, requested_file_name.c_str(), hash_results);

  if(!compare_hashes(hash, hash_results)){
    Serial.println("The file recieved is not correct. Deleting file...");
    deleteFile(SD, requested_file_name.c_str());
  }
  else Serial.println("Hashes match! Download was succesful.");
  listDir(SD, "/", 1);
}

void prepare_file(String file_name){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  if (check_file(SD,file_name.c_str()) < 0) {
    Serial.println("File is not found. Exiting");
    return;
  }
  String encrypted_filed_name = file_name + ".ascon";
  Serial.printf("Encrypting file %s ...\n", file_name);
  deleteFile(SD, encrypted_filed_name.c_str());
  encrypt_file_2(SD,file_name.c_str(), encrypted_filed_name.c_str());
  send_hash(SD, file_name.c_str());
  send_file(SD, encrypted_filed_name.c_str(), file_name.c_str());
  deleteFile(SD, encrypted_filed_name.c_str());
}


int encrypt_message(char* plaintext, char* cyphertext, size_t *clen, size_t mlen, unsigned char *npub){
  unsigned char *m = (unsigned char*)plaintext;
  unsigned char ad[] = "";
  unsigned long long adlen = strlen((char*)ad);

  ascon128_aead_encrypt((unsigned char*)cyphertext, clen, m, mlen, ad, adlen, (const char unsigned*)npub, k);
  return 0;
}

long long unsigned int decrypt_message(char *ciphertext,size_t clen ,char *plaintext, size_t* decrypted_mlen, const unsigned char* npub){
  unsigned char ad[] = "";  // Optional associated data
  unsigned long long adlen = strlen((char*)ad);
  int decrypt_status = ascon128_aead_decrypt((unsigned char*)plaintext, decrypted_mlen, (const unsigned char*)ciphertext, clen, ad, adlen, npub, k);
  if (decrypt_status < 0) {
    Serial.println("Decryption failed!");
    return -1;
  }
  return *decrypted_mlen;
}

