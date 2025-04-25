
void receive_hash(size_t msg_length, uint8_t* buffer) {
  if (msg_length < 1 + ASCON128_NONCE_SIZE) {
    Serial.println("Invalid hash message size");
    return;
  }

  memcpy(current_nonce, buffer + 1, ASCON128_NONCE_SIZE);
  size_t nonce_int = nonce_to_integer(current_nonce);
  if (validate_nonce(nonce_int) == -1) {
    return;
  }

  size_t clen = msg_length - 1 - ASCON128_NONCE_SIZE;
  const char* encrypted_hash = (const char*)(buffer + 1 + ASCON128_NONCE_SIZE);

  char decrypted_hash[CRYPTO_BYTES] = {0};
  size_t decrypted_len = 0;

  int result = decrypt_message((char*)encrypted_hash, clen, decrypted_hash, &decrypted_len, current_nonce);
  if (result < 0) {
    Serial.println("Decryption failed in receive_hash()");
    return;
  }

  memcpy(hash, decrypted_hash, CRYPTO_BYTES);
  return;
}


void recieve_encrypted_chunk(size_t msg_length, uint8_t *buffer){
  if (msg_length < sizeof(uint8_t) + sizeof(size_t) + 1) {
    Serial.println("Invalid data message size");
    return;
  }

  uint8_t last_chunk;
  size_t chunk_size;
  memcpy(&last_chunk, buffer + 1, sizeof(uint8_t));
  memcpy(&chunk_size, buffer + 2, sizeof(size_t));

  if (msg_length != sizeof(uint8_t) + sizeof(uint8_t) + sizeof(size_t) + chunk_size) {
    Serial.println("Invalid chunk payload size");
    return;
  }

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  Serial.printf("%d bytes received, reading it into %s.ascon ...\n",chunk_size, requested_file_name);
  appendFile(SD, (requested_file_name + ".ascon").c_str(), (const char*)(buffer + 2 + sizeof(size_t)), chunk_size);
  if (last_chunk){
    decrypt_verify(requested_file_name + ".ascon");
  }
  return; 
}

int validate_nonce(size_t nonce){
  if(nonce > MAX_NONCES) return -1;
  if(nonces_table[nonce] == 1) return -1;
  else nonces_table[nonce] = 1;
  return 0;
}

int decrypt_request(const char* ciphertext, size_t clen, char* plaintext, const unsigned char* nonce) {
  size_t decrypted_mlen = 0;

  int result = decrypt_message((char*)ciphertext ,clen, plaintext, &decrypted_mlen, nonce);

  if (result < 0) {
    Serial.println("Decryption failed in decrypt_request()");
    return -1;
  }

  plaintext[(decrypted_mlen < 8191) ? decrypted_mlen : 8191] = '\0';
  Serial.print("[Server] Decrypted message: ");
  Serial.println(plaintext);

  parse_request(String(plaintext));
  return 0;
}


void recieve_request(size_t msg_length, uint8_t* buffer) {
  if (msg_length < 1 + ASCON128_NONCE_SIZE) {
    Serial.println("Invalid message length");
    return;
  }

  memcpy(current_nonce, buffer + 1, ASCON128_NONCE_SIZE);
  size_t nonce_int = nonce_to_integer(current_nonce);
  if (validate_nonce(nonce_int) == -1) return;

  size_t msg_content_length = msg_length - 1 - ASCON128_NONCE_SIZE;
  const char* encrypted_msg = (const char*)(buffer + 1 + ASCON128_NONCE_SIZE);

  char decrypted_plaintext[100];
  if (decrypt_request(encrypted_msg, msg_content_length, decrypted_plaintext, (const unsigned char*)current_nonce) < 0) {
    Serial.println("Failed to decrypt incoming request.");
  }
}

void parse_input(size_t msg_length ,WiFiClient& client){
  uint8_t* buffer = (uint8_t*)malloc(msg_length);
  if (!buffer) {
    Serial.println("Memory allocation failed");
    return;
  }
  client.readBytes((char*)buffer, msg_length);
  uint8_t msg_type = buffer[0]; // the metadata contains what message type it is

  switch (msg_type) {
    case hash_code:
      receive_hash(msg_length, buffer);
      break;
    case data_code:
      recieve_encrypted_chunk(msg_length, buffer);
      break;
    case msg_code:
      recieve_request(msg_length, buffer);
      break;
    default:
      Serial.printf("Unknown msg_type: %u\n", msg_type);
      break;
  }

  free(buffer);
}

void recieve_input(WiFiClient& client) { 
  while (client.available() >= sizeof(size_t)) {
    size_t msg_length;
    client.readBytes((char*)&msg_length, sizeof(size_t)); //reads the metadata, the first part contains the size of the message being recieved

    unsigned long start = millis();
    while (client.available() < msg_length) { //waits until the client recieves the full message length 
      if (millis() - start > 10000) { // Time out so it does not hang
        Serial.println("Timeout waiting for full message");
        return;
      }
      delay(20);
    }
    parse_input(msg_length, client);

  }
}

int parse_request(String msg) {
  if (msg.startsWith("/GET")) {
    String file_name = get_file_name(msg);
    if (file_name == "") {
      Serial.printf("File name could not be retrieved\n");
      return -1;
    }
    prepare_file(file_name);
    return 0;
  }
  Serial.println("Command is not recognised");
  return -1;
}
