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
  encrypt_file(file_name.c_str(), &SD, encrypted_filed_name.c_str());
  send_hash(SD, file_name.c_str());
  send_file(SD, encrypted_filed_name.c_str(), file_name.c_str());
  listDir(SD, "/", 1);
  deleteFile(SD, encrypted_filed_name.c_str());
}


void decrypt_verify(String file){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  deleteFile(SD, requested_file_name.c_str());
  int result = decrypt_file((requested_file_name + ".ascon").c_str(),&SD,requested_file_name.c_str());
  //deleteFile(SD, (requested_file_name + ".ascon").c_str());
  char unsigned  hash[CRYPTO_BYTES] = { 0 };
  hash_file(SD, requested_file_name.c_str(), hash);
  print_hash_output(4, hash);
  listDir(SD, "/", 1);
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
          recieve_input(client);
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
      send_request(input);
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
