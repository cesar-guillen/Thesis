#define CHUNK_SIZE 512

void decrypt_verify(String file){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  deleteFile(SD, requested_file_name.c_str());
  int result = decrypt_file((requested_file_name + ".ascon").c_str(),&SD,requested_file_name.c_str());
  deleteFile(SD, (requested_file_name + ".ascon").c_str());
  char unsigned  hash[CRYPTO_BYTES] = { 0 };
  hash_file(SD, requested_file_name.c_str(), hash);
  print_hash_output(4, hash);
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
  encrypt_file(file_name.c_str(), &SD, encrypted_filed_name.c_str());
  send_hash(SD, file_name.c_str());
  send_file(SD, encrypted_filed_name.c_str(), file_name.c_str());
  listDir(SD, "/", 1);
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

void print_hex(const unsigned char* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char buf[3];
    sprintf(buf, "%02X", data[i]);
    Serial.printf("%s ",buf);
  }
  Serial.println();
}
