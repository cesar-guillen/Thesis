#define CHUNK_SIZE 512

void send_request(String input){
  Serial.print("[Client] Sending message: ");
  Serial.println(input);

  const char* msg = input.c_str();
  size_t msg_len = strlen(msg);

  size_t total_size = sizeof(msg_code) + msg_len + 1;
  char* buffer = (char*)malloc(sizeof(size_t) + total_size);
  if (!buffer) {
    Serial.println("Failed to allocate memory for message");
    return;
  }

  size_t offset = 0;
  memcpy(buffer + offset, &total_size, sizeof(total_size)); offset += sizeof(total_size);
  memcpy(buffer + offset, &msg_code, sizeof(msg_code)); offset += sizeof(msg_code);
  memcpy(buffer + offset, msg, msg_len); offset += msg_len;
  buffer[offset] = '\n';

  persistentClient.write((uint8_t*)buffer, sizeof(size_t) + total_size);
  free(buffer);
  
  requested_file_name = get_file_name(input);
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    free(buffer);
    return;
  }
}


void send_hash(fs::FS &fs, const char *original_file) {
  unsigned char hash[CRYPTO_BYTES] = { 0 };
  hash_file(fs, original_file, hash);
  print_hash_output(4, hash);

  size_t payload_size = sizeof(hash_code) + CRYPTO_BYTES;
  size_t total_size = sizeof(size_t) + payload_size; 

  uint8_t *payload = (uint8_t *)malloc(total_size);
  if (payload == nullptr) {
    Serial.println("Memory allocation failed");
    return;
  }
  size_t offset = 0;
  memcpy(payload + offset, &payload_size, sizeof(payload_size)); offset += sizeof(payload_size);
  memcpy(payload + offset, &hash_code, sizeof(hash_code)); offset += sizeof(hash_code);
  memcpy(payload + offset, hash, CRYPTO_BYTES);
  persistentClient.write(payload, total_size);
  free(payload);
}


void send_file(fs::FS &fs, const char *encrypted_file, const char* original_file) {
  File file = fs.open(encrypted_file);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  size_t file_size = file.size();
  size_t file_sent = 0;

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
    Serial.printf("Sending a chunk of size: %d", total_packet_size);
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
    persistentClient.flush();
    delay(500);
    file_sent += size_to_send;
    free(buffer);
  }

  file.close();
}

