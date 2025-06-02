#define CHUNK_SIZE 512

void decrypt_verify(String file){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  deleteFile(SD, requested_file_name.c_str());
  Serial.println("Decrypting file ...");
  int result = decrypt_file((requested_file_name + ".ascon").c_str(),&SD,requested_file_name.c_str());
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
  encrypt_file(file_name.c_str(), &SD, encrypted_filed_name.c_str());
  send_hash(SD, file_name.c_str());
  send_file(SD, encrypted_filed_name.c_str(), file_name.c_str());
  deleteFile(SD, encrypted_filed_name.c_str());
}


int encrypt_message(char* plaintext, char* cyphertext, size_t *clen, size_t mlen, unsigned char *npub, uint8_t msg_code) {
  unsigned char *m = (unsigned char*)plaintext;

  size_t adlen = sizeof(msg_code) + sizeof(size_t) + ASCON128_NONCE_SIZE;
  unsigned char ad[adlen];

  size_t offset = 0;
  memcpy(ad + offset, &msg_code, sizeof(msg_code)); offset += sizeof(msg_code);
  memcpy(ad + offset, &mlen, sizeof(size_t)); offset += sizeof(size_t);
  memcpy(ad + offset, npub, ASCON128_NONCE_SIZE);

  ascon128_aead_encrypt((unsigned char*)cyphertext, clen, m, mlen, ad, adlen, (const unsigned char*)npub, k);
  return 0;
}


long long unsigned int decrypt_message(char *ciphertext, size_t clen, char *plaintext, size_t* decrypted_mlen, const unsigned char* npub, uint8_t msg_code, size_t expected_mlen) {
  size_t adlen = sizeof(msg_code) + sizeof(size_t) + ASCON128_NONCE_SIZE;
  unsigned char ad[adlen];

  size_t offset = 0;
  memcpy(ad + offset, &msg_code, sizeof(msg_code)); offset += sizeof(msg_code);
  memcpy(ad + offset, &expected_mlen, sizeof(size_t)); offset += sizeof(size_t);
  memcpy(ad + offset, npub, ASCON128_NONCE_SIZE);

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
