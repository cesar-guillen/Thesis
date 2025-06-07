#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "mbedtls/chacha20.h"

#define IV_SIZE 12      // ChaCha20 uses a 96-bit (12-byte) nonce

const uint8_t iv[IV_SIZE] = {0}; // should be randomized per file ideally
unsigned char firsthash[HASH_SIZE] = {0};
unsigned char secondhash[HASH_SIZE] = {0};

void encrypt_file(fs::FS &fs, const char *inputPath, const char *outputPath) {
  File inFile = fs.open(inputPath, "r");
  File outFile = fs.open(outputPath, "w");

  if (!inFile || !outFile) {
    Serial.println("File open failed");
    return;
  }

  size_t originalSize = inFile.size(); 
  outFile.write((uint8_t*)&originalSize, sizeof(originalSize));

  mbedtls_chacha20_context chacha;
  mbedtls_chacha20_init(&chacha);
  mbedtls_chacha20_setkey(&chacha, key);

  uint8_t iv_copy[IV_SIZE];
  memcpy(iv_copy, iv, IV_SIZE);
  mbedtls_chacha20_starts(&chacha, iv_copy, 0); // counter starts at 0

  uint8_t buffer[CHUNK_SIZE];
  uint8_t output[CHUNK_SIZE];
  unsigned long start_time = millis();

  while (inFile.available()) {
    size_t readLen = inFile.read(buffer, CHUNK_SIZE);
    mbedtls_chacha20_update(&chacha, readLen, buffer, output);
    outFile.write(output, readLen);
  }

  unsigned long end_time = millis();
  mbedtls_chacha20_free(&chacha);
  inFile.close();
  outFile.close();
}

void decrypt_file(fs::FS &fs, const char *inputPath, const char *outputPath) {
  File inFile = fs.open(inputPath, "r");
  File outFile = fs.open(outputPath, "w");

  if (!inFile || !outFile) {
    Serial.println("File open failed");
    return;
  }

  size_t originalSize = 0;
  inFile.read((uint8_t*)&originalSize, sizeof(originalSize));

  mbedtls_chacha20_context chacha;
  mbedtls_chacha20_init(&chacha);
  mbedtls_chacha20_setkey(&chacha, key);

  uint8_t iv_copy[IV_SIZE];
  memcpy(iv_copy, iv, IV_SIZE);
  mbedtls_chacha20_starts(&chacha, iv_copy, 0); // counter = 0

  uint8_t buffer[CHUNK_SIZE];
  uint8_t output[CHUNK_SIZE];
  size_t totalWritten = 0;

  unsigned long start_time = millis();
  while (inFile.available() && totalWritten < originalSize) {
    size_t readLen = inFile.read(buffer, CHUNK_SIZE);
    mbedtls_chacha20_update(&chacha, readLen, buffer, output);

    size_t toWrite = (originalSize - totalWritten > readLen) ? readLen : (originalSize - totalWritten);
    outFile.write(output, toWrite);
    totalWritten += toWrite;
  }

  unsigned long end_time = millis();
  mbedtls_chacha20_free(&chacha);
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
  decrypt_file(SD, (requested_file_name + ".ascon").c_str(),requested_file_name.c_str());
  deleteFile(SD, (requested_file_name + ".ascon").c_str());
  char unsigned  hash_results[HASH_SIZE] = { 0 };
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
  encrypt_file(SD,file_name.c_str(), encrypted_filed_name.c_str());
  send_hash(SD, file_name.c_str());
  send_file(SD, encrypted_filed_name.c_str(), file_name.c_str());
  deleteFile(SD, encrypted_filed_name.c_str());
}

int encrypt_message(char* plaintext, char* ciphertext, size_t* clen, size_t mlen,
                    const unsigned char* npub, uint8_t msg_code) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }

    // Prepare associated data
    unsigned char ad[1 + sizeof(size_t) + NONCE_SIZE];
    size_t offset = 0;
    memcpy(ad + offset, &msg_code, sizeof(msg_code)); offset += sizeof(msg_code);
    memcpy(ad + offset, &mlen, sizeof(mlen)); offset += sizeof(mlen);
    memcpy(ad + offset, npub, NONCE_SIZE);
    size_t adlen = offset;

    unsigned char tag[16];

    if (mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, mlen,
                                  npub, 12,  // 12 is standard nonce size for GCM
                                  ad, adlen,
                                  (unsigned char*)plaintext,
                                  (unsigned char*)ciphertext,
                                  16, tag) != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }

    memcpy(ciphertext + mlen, tag, 16);
    *clen = mlen + 16;

    mbedtls_gcm_free(&gcm);
    return 0;
}



int decrypt_message(char* ciphertext, size_t clen, char* plaintext,
                    size_t* decrypted_mlen, const unsigned char* npub,
                    uint8_t msg_code, size_t mlen_expected) {
    if (clen < 16) return -1;

    size_t ctext_len = clen - 16;
    unsigned char tag[16];
    memcpy(tag, ciphertext + ctext_len, 16);

    unsigned char* tmp_ctext = (unsigned char*)malloc(ctext_len);
    if (!tmp_ctext) return -1;
    memcpy(tmp_ctext, ciphertext, ctext_len);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
        free(tmp_ctext);
        mbedtls_gcm_free(&gcm);
        return -1;
    }

    // Recreate associated data
    unsigned char ad[1 + sizeof(size_t) + NONCE_SIZE];
    size_t offset = 0;
    memcpy(ad + offset, &msg_code, sizeof(msg_code)); offset += sizeof(msg_code);
    memcpy(ad + offset, &mlen_expected, sizeof(mlen_expected)); offset += sizeof(mlen_expected);
    memcpy(ad + offset, npub, NONCE_SIZE);
    size_t adlen = offset;

    int ret = mbedtls_gcm_auth_decrypt(&gcm, ctext_len,
                                       npub, 12,
                                       ad, adlen,
                                       tag, 16,
                                       tmp_ctext, (unsigned char*)plaintext);

    free(tmp_ctext);
    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        Serial.println("Decryption failed!");
        return -1;
    }

    *decrypted_mlen = ctext_len;
    return *decrypted_mlen;
}


void increment_nonce(unsigned char nonce[NONCE_SIZE]) {
    for (int i = NONCE_SIZE - 1; i >= 0; i--) {
        nonce[i]++;
        if (nonce[i] != 0) {
            break;
        }
    }
}
