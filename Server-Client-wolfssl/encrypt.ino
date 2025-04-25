#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <string.h>
#define CHUNK_SIZE 16
#define IV_SIZE 16

const uint8_t iv[IV_SIZE] = { 0 };
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

    Aes aes;
    byte iv_copy[16];
    memcpy(iv_copy, iv, 16);

    wc_AesSetKey(&aes, key, 16, iv_copy, AES_ENCRYPTION);

    byte buffer[16];
    byte output[16];

    while (inFile.available()) {
        size_t readLen = inFile.read(buffer, 16);
        if (readLen < 16) {
            byte pad = 16 - readLen;
            memset(buffer + readLen, pad, pad);
        }
        wc_AesCbcEncrypt(&aes, output, buffer, 16);
        outFile.write(output, 16);
    }

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

    Aes aes;
    byte iv_copy[16];
    memcpy(iv_copy, iv, 16);

    wc_AesSetKey(&aes, key, 16, iv_copy, AES_DECRYPTION);

    byte buffer[16];
    byte output[16];
    size_t totalWritten = 0;

    while (inFile.available() && totalWritten < originalSize) {
        size_t readLen = inFile.read(buffer, 16);
        if (readLen != 16) break;

        wc_AesCbcDecrypt(&aes, output, buffer, 16);

        size_t bytesToWrite = (originalSize - totalWritten) > 16 ? 16 : (originalSize - totalWritten);
        outFile.write(output, bytesToWrite);
        totalWritten += bytesToWrite;
    }

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

int encrypt_message(char* plaintext, char* ciphertext, size_t* clen, size_t mlen, const unsigned char* npub) {
    Aes aes;
    int ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0) return -1;

    ret = wc_AesGcmSetKey(&aes, key, 32);
    if (ret != 0) {
        wc_AesFree(&aes);
        return -1;
    }

    unsigned char tag[16];
    unsigned char ad[] = "";
    word32 adlen = 0;

    ret = wc_AesGcmEncrypt(&aes,
                           (byte*)ciphertext,              
                           (const byte*)plaintext, mlen, npub, 12, tag, 16, ad, adlen); 

    if (ret != 0) {
        wc_AesFree(&aes);
        Serial.printf("encryption failed");
        return -1;
    }

    memcpy(ciphertext + mlen, tag, 16);
    *clen = mlen + 16;

    wc_AesFree(&aes);
    return 0;
}



int decrypt_message(char* ciphertext, size_t clen, char* plaintext, size_t* decrypted_mlen, const unsigned char* npub) {
    if (clen < 16) return -1;

    size_t ctext_len = clen - 16;
    unsigned char tag[16];
    memcpy(tag, ciphertext + ctext_len, 16);

    Aes aes;
    int ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0) return -1;

    ret = wc_AesGcmSetKey(&aes, key, 32);
    if (ret != 0) {
        wc_AesFree(&aes);
        return -1;
    }

    unsigned char ad[] = "";
    word32 adlen = 0;

    ret = wc_AesGcmDecrypt(&aes,
                           (byte*)plaintext,                
                           (const byte*)ciphertext, ctext_len, npub, 12, tag, 16, ad, adlen);                       

    wc_AesFree(&aes);

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
