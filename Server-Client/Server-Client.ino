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
  size_t payload_size =  CRYPTO_BYTES + sizeof(hash_code);
  uint8_t *payload = (uint8_t *)malloc(payload_size);
  if (payload == nullptr) {
    Serial.println("Memory allocation failed");
    return;
  }
  memcpy(payload, &hash_code, sizeof(hash_code));
  memcpy(payload + sizeof(hash_code), hash, CRYPTO_BYTES);
  persistentClient.write(payload, payload_size);
  free(payload);
}

void send_file(fs::FS &fs, const char *encrypted_file, const char* original_file){
  File file = fs.open(encrypted_file);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }
  size_t file_size = file.size();
  size_t file_sent = 0;
  while (file_sent < file_size)
  {
    size_t size_to_send = ((file_size - file_sent) > CHUNK_SIZE ? CHUNK_SIZE : (file_size - file_sent));
    char *curr_chunk = (char *)malloc(size_to_send + sizeof(data_code) + sizeof(size_to_send));
    if (curr_chunk == nullptr) {
      Serial.println("Memory allocation failed");
      file.close();
      return;
    }
    memcpy(curr_chunk, &data_code, sizeof(data_code));
    memcpy(curr_chunk + sizeof(data_code), &size_to_send, sizeof(size_to_send));
    file.readBytes(curr_chunk + sizeof(size_to_send) + sizeof(data_code), size_to_send);
    persistentClient.write((uint8_t*)curr_chunk, size_to_send + sizeof(size_to_send) + sizeof(data_code));
    file_sent += size_to_send;
    free(curr_chunk);
  }
  file.close();
  return;
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
  if (msg.startsWith("GET")) {
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

void parse_input(WiFiClient& client) {
  uint8_t msg_type = 2;
  client.readBytes((char*)&msg_type, sizeof(msg_type));
  if (msg_type == hash_code) {
    Serial.println("hash received, reading it into hash...");
    if (client.available() >= CRYPTO_BYTES) {
      client.readBytes((char*)hash, CRYPTO_BYTES);
      print_hash_output(4, hash);
    }
  }
  else if (msg_type == data_code) {
    if (!SD.begin()) {
      Serial.println("Card Mount Failed");
      return;
    }
    Serial.printf("DATA received, reading it into %s.ascon ...\n",  requested_file_name);
    size_t size_to_read;
    if (client.readBytes((char*)&size_to_read, sizeof(size_t)) != sizeof(size_t)) {
      Serial.println("Failed to read chunk size");
      return;
    }
    unsigned char* curr_chunk = (unsigned char*)malloc(size_to_read);
    if (client.readBytes((char*)curr_chunk, size_to_read) != size_to_read) {
      Serial.println("Failed to read full chunk");
      free(curr_chunk);
      return;
    }
    appendFile(SD, (requested_file_name+".ascon1").c_str(), (const char*)curr_chunk, size_to_read);
    free(curr_chunk);
  }
  else if (msg_type == msg_code) {
    String msg = client.readStringUntil('\n');
    Serial.print("[Server] Received: ");
    Serial.println(msg);
    parse_request(msg);
  }
  Serial.printf("msg_type is : %u\n", msg_type);
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

    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim(); // Remove newline/extra whitespace
      if (input.length() > 0) {
        Serial.print("[Client] Sending message: ");
        Serial.println(input);
        persistentClient.println(input); 
        requested_file_name = get_file_name(input);
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
  // Nothing here; tasks handle everything
}
