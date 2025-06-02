#define CHUNK_SIZE 512
#define MAX_ENCRYPTED_MSG_SIZE 100

void send_request(String input) {
  Serial.print("[Client] Sending message: ");
  Serial.println(input);

  char* msg = (char*)input.c_str();
  size_t msg_len = strlen(msg);
  char encrypted_msg[MAX_ENCRYPTED_MSG_SIZE];
  size_t clen = 0;
  encrypt_message(msg, encrypted_msg, &clen, msg_len, npub, msg_code);
  size_t total_payload_size = sizeof(msg_code) + ASCON128_NONCE_SIZE + clen;
  size_t total_packet_size = sizeof(size_t) + total_payload_size;

  char* buffer = (char*)malloc(total_packet_size);
  if (!buffer) {
    Serial.println("Failed to allocate memory for message");
    return;
  }

  size_t offset = 0;
  memcpy(buffer + offset, &total_payload_size, sizeof(total_payload_size)); offset += sizeof(total_payload_size);
  memcpy(buffer + offset, &msg_code, sizeof(msg_code)); offset += sizeof(msg_code);
  memcpy(buffer + offset, npub, ASCON128_NONCE_SIZE); offset += ASCON128_NONCE_SIZE;
  memcpy(buffer + offset, encrypted_msg, clen); offset += clen;
  persistentClient.write((uint8_t*)buffer, total_packet_size);
  free(buffer);
  ascon_aead_increment_nonce(npub);
  requested_file_name = get_file_name(input);
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  deleteFile(SD, (requested_file_name + ".ascon").c_str());
}


void send_hash(fs::FS &fs, const char *original_file) {
  unsigned char hash[CRYPTO_BYTES] = { 0 };
  hash_file(fs, original_file, hash);
  print_hash_output(4, hash);
  // Encrypt the hash
  char encrypted_hash[MAX_ENCRYPTED_MSG_SIZE] = { 0 };
  size_t clen = 0;
  encrypt_message((char*)hash, encrypted_hash, &clen, CRYPTO_BYTES, npub, hash_code);

  size_t payload_size = sizeof(hash_code) + ASCON128_NONCE_SIZE + clen;
  size_t total_size = sizeof(size_t) + payload_size;

  uint8_t *payload = (uint8_t *)malloc(total_size);
  if (payload == nullptr) {
    Serial.println("Memory allocation failed");
    return;
  }

  size_t offset = 0;
  memcpy(payload + offset, &payload_size, sizeof(payload_size)); offset += sizeof(payload_size);
  memcpy(payload + offset, &hash_code, sizeof(hash_code)); offset += sizeof(hash_code);
  memcpy(payload + offset, npub, ASCON128_NONCE_SIZE); offset += ASCON128_NONCE_SIZE;
  memcpy(payload + offset, encrypted_hash, clen); offset += clen;

  persistentClient.write(payload, total_size);
  free(payload);

  ascon_aead_increment_nonce(npub);
}



void send_file(fs::FS &fs, const char *encrypted_file, const char* original_file) {
  File file = fs.open(encrypted_file);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  size_t file_size = file.size();
  size_t file_sent = 0;
  Serial.println("Sending file ...");
  while (file_sent < file_size) {
    size_t size_to_send = ((file_size - file_sent) > CHUNK_SIZE ? CHUNK_SIZE : (file_size - file_sent));
    uint8_t last_chunk = (file_sent + size_to_send == file_size) ? 1 : 0;
    size_t payload_size = sizeof(data_code) + sizeof(last_chunk) + sizeof(size_to_send) + size_to_send;
    size_t total_message_size = sizeof(data_code) + sizeof(last_chunk) + sizeof(size_to_send) + size_to_send;
    size_t total_packet_size = sizeof(size_t) + total_message_size;

    char *buffer = (char *)malloc(total_packet_size);
    if (!buffer) {
      Serial.println("Memory allocation failed");
      file.close();
      return;
    }

    size_t offset = 0;
    memcpy(buffer + offset, &total_message_size, sizeof(total_message_size)); offset += sizeof(total_message_size);
    memcpy(buffer + offset, &data_code, sizeof(data_code)); offset += sizeof(data_code);
    memcpy(buffer + offset, &last_chunk, sizeof(last_chunk)); offset += sizeof(last_chunk);
    memcpy(buffer + offset, &size_to_send, sizeof(size_to_send)); offset += sizeof(size_to_send);
    file.readBytes(buffer + offset, size_to_send);
    int bytes_written = persistentClient.write((uint8_t*)buffer, total_packet_size);

    if (bytes_written != total_packet_size) {
      Serial.printf("Failed to write full chunk: wrote %d of %d bytes\n", bytes_written, total_packet_size);
    }
    delay(200); //delay to not overwhelm the client
    file_sent += size_to_send;
    float percent = ((float)file_sent / (float)file_size) * 100.0f;
    Serial.printf("Progress: %.2f%%\n", percent);
    free(buffer);
  }

  file.close();
}

