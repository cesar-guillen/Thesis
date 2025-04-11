#include <WiFi.h>
#include <ASCON.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <stdio.h>
#include <string.h>
#include "../ascon-suite-master/apps/asconcrypt/fileops.h"
#include "../ascon-suite-master/apps/asconcrypt/readpass.h"
#include "../ascon-suite-master/apps/asconcrypt/readpass.c"
#include "../ascon-suite-master/apps/asconcrypt/fileops.c"
#include "../ascon-suite-master/apps/asconcrypt/asconcrypt.c"

extern "C" {
  #include "esp_task_wdt.h"
}

// Server config
const char* ssid = "esp";
const char* password = "MyWifiZone123!";
WiFiServer server(5000);
#define CHUNK_SIZE 1024
#define CRYPTO_BYTES 64
WiFiClient persistentClient;

//const char* remoteIP = "192.168.244.201"; // red
const char* remoteIP = "192.168.244.196"; // white
const uint16_t remotePort = 5000;

// global variables
char unsigned  hash[CRYPTO_BYTES] = { 0 };
String requested_file_name = "";
const uint8_t hash_code = 1;
const uint8_t data_code = 0;
const uint8_t msg_code = 47;

// Task handles
TaskHandle_t serverTaskHandle = NULL;
TaskHandle_t clientTaskHandle = NULL;

String get_file_name(String msg) {
  msg.trim();
  int firstSpace = msg.indexOf(' ');
  if (firstSpace == -1) {
    return "";
  }
  String secondWord = msg.substring(firstSpace + 1);
  int secondSpace = secondWord.indexOf(' ');
  if (secondSpace != -1) {
    return secondWord.substring(0, secondSpace);
  }
  return "/" + secondWord;
}

int check_file(fs::FS &fs, const char *path){
  File file = fs.open(path);
  if (!file) {
    Serial.println("File does not exist");
    return -1;
  }
  file.close();
  return 0;
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
    size_t total_message_size = payload_size;

    char *buffer = (char *)malloc(sizeof(total_message_size) + payload_size);
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
    Serial.println("Sending a chunk!");
    persistentClient.write((uint8_t*)buffer, sizeof(total_message_size) + payload_size);
    file_sent += size_to_send;
    free(buffer);
  }

  file.close();
}


void prepare_file(String file_name){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  if (check_file(SD,file_name.c_str()) < 0) {
    return;
  }
  String encrypted_filed_name = file_name + ".ascon";
  Serial.printf("Encrypting file %s ...\n", file_name);
  encrypt_file(file_name.c_str(), &SD, encrypted_filed_name.c_str());
  send_hash(SD, file_name.c_str());
  send_file(SD, encrypted_filed_name.c_str(), file_name.c_str());
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
  return -1;
}

void decrypt_verify(String file){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  int result = decrypt_file((requested_file_name + ".ascon").c_str(),&SD,requested_file_name.c_str());
  char unsigned  hash[CRYPTO_BYTES] = { 0 };
  hash_file(SD, requested_file_name.c_str(), hash);
  print_hash_output(4, hash);
  listDir(SD, "/", 1);
}

void parse_input(WiFiClient& client) { 
  while (client.available() >= sizeof(size_t)) {
    size_t msg_length;
    client.readBytes((char*)&msg_length, sizeof(size_t)); //reads the metadata, the first part contains the size of the message being recieved

    unsigned long start = millis();
    while (client.available() < msg_length) { //waits until the client recieves the full message length 
      if (millis() - start > 10000) { // Time out so it does not hang
        Serial.println("Timeout waiting for full message");
        return;
      }
      delay(10);
    }

    uint8_t* buffer = (uint8_t*)malloc(msg_length);
    if (!buffer) {
      Serial.println("Memory allocation failed");
      return;
    }
    client.readBytes((char*)buffer, msg_length);

    uint8_t msg_type = buffer[0]; //the metadata contains what message type it is. 
    Serial.printf("Finished parsing message of type: %u\n", msg_type);

    if (msg_type == hash_code) {
      if (msg_length != sizeof(uint8_t) + CRYPTO_BYTES) {
        Serial.println("Invalid hash message size");
        free(buffer);
        return;
      }
      memcpy(hash, buffer + 1, CRYPTO_BYTES);
      print_hash_output(4, hash);
    }

    else if (msg_type == data_code) {
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

      if (last_chunk) Serial.println("LAST CHUNK IS RECEIVED!");
      Serial.printf("DATA received, reading it into %s.ascon ...\n", requested_file_name);
      appendFile(SD, (requested_file_name + ".ascon").c_str(), (const char*)(buffer + 2 + sizeof(size_t)), chunk_size);
      if (last_chunk){
        Serial.println("LAST CHUNK IS RECEIVED!");
        decrypt_verify(requested_file_name + ".ascon");
      } 
    }

    else if (msg_type == msg_code) {
      size_t msg_content_length = msg_length - 1;
      String msg = String((char*)(buffer + 1), msg_content_length);

      Serial.print("[Server] Received: ");
      Serial.println(msg);
      parse_request(msg);
    }

    else {
      Serial.printf("Unknown msg_type: %u\n", msg_type);
    }

    free(buffer);
  }
}

void serverTask(void* parameter) {
  server.begin();
  Serial.println("[Server] Started on port 5000");

  while (true) {
    WiFiClient client = server.available();
    if (client) {
      Serial.println("[Server] Client connected");
      while (client.connected()) {
        if (client.available()) {
          parse_input(client);
        } else {
          delay(1);  
        }
      }
      client.stop();
      Serial.println("[Server] Client disconnected");
    }
    delay(10); 
  }
}

void clientTask(void* parameter) {
  delay(5000);  // Wait for WiFi and server to start

  while (true) {
    if (!persistentClient.connected()) {
      Serial.println("[Client] Connecting to remote...");
      if (persistentClient.connect(remoteIP, remotePort)) {
        Serial.println("[Client] Connected.");
      } else {
        Serial.println("[Client] Failed to connect, retrying in 5 seconds...");
        delay(5000);
        continue;
      }
    }
    if (Serial.available()) { // the following code reads the text from the terminal and sends it to the new client, it also appends the neccesary metadata.
      String input = Serial.readStringUntil('\n');
      input.trim(); 
      if (input.length() > 0) {
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
        deleteFile(SD, (requested_file_name + ".ascon").c_str());
      }
    }
    delay(100); 
  }
}


void setup() {
  Serial.begin(115200);
  esp_task_wdt_deinit(); // watchdog cries without this. It believes funcitons get stuck when they do a lot of computing power.
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  // Start tasks
  xTaskCreatePinnedToCore(serverTask,"Server Task",16384,NULL,1,&serverTaskHandle,1);
  xTaskCreatePinnedToCore(clientTask,"Client Task",8192,NULL,1,&clientTaskHandle,0);
}

void loop() {
}
