
void receive_hash(size_t msg_length, uint8_t* buffer){
  if (msg_length != sizeof(uint8_t) + CRYPTO_BYTES) {
    Serial.println("Invalid hash message size");
    free(buffer);
    return;
  }
  memcpy(hash, buffer + 1, CRYPTO_BYTES);
  print_hash_output(4, hash);
}

void recieve_encrypted_chunk(size_t msg_length, uint8_t *buffer){
  if (msg_length < sizeof(uint8_t) + sizeof(size_t) + 1) {
    Serial.println("Invalid data message size");
    free(buffer);
    return;
  }

  uint8_t last_chunk;
  size_t chunk_size;
  memcpy(&last_chunk, buffer + 1, sizeof(uint8_t));
  memcpy(&chunk_size, buffer + 2, sizeof(size_t));

  if (msg_length != sizeof(uint8_t) + sizeof(uint8_t) + sizeof(size_t) + chunk_size) {
    Serial.println("Invalid chunk payload size");
    free(buffer);
    return;
  }

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    free(buffer);
    return;
  }
  Serial.printf("DATA received, reading it into %s.ascon ...\n", requested_file_name);
  appendFile(SD, (requested_file_name + ".ascon").c_str(), (const char*)(buffer + 2 + sizeof(size_t)), chunk_size);
  if (last_chunk){
    Serial.println("LAST CHUNK IS RECEIVED!");
    decrypt_verify(requested_file_name + ".ascon");
  }
  return; 
}

void recieve_request(size_t msg_length, uint8_t* buffer){
  size_t msg_content_length = msg_length - 1;
  String msg = String((char*)(buffer + 1), msg_content_length);

  Serial.print("[Server] Received: ");
  Serial.println(msg);
  parse_request(msg);
}

void parse_input(size_t msg_length ,WiFiClient& client){
  uint8_t* buffer = (uint8_t*)malloc(msg_length);
  if (!buffer) {
    Serial.println("Memory allocation failed");
    return;
  }
  client.readBytes((char*)buffer, msg_length);
  uint8_t msg_type = buffer[0]; // the metadata contains what message type it is
  Serial.printf("Finished parsing message of type: %u\n", msg_type);

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
    Serial.println("Command recognized: GET");
    String file_name = get_file_name(msg);
    if (file_name == "") {
      Serial.printf("File name could not be retrieved\n");
      return -1;
    }
    Serial.printf("File name is: %s\n", file_name);
    prepare_file(file_name);
    return 0;
  }
  Serial.println("Command is not recognised");
  return -1;
}
