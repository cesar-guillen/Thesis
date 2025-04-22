#include <WiFi.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <stdio.h>
#include <string.h>

extern "C" {
  #include "esp_task_wdt.h"
}
#define NONCE_SIZE 12
#define HASH_SIZE 32
#define MAX_NONCES 2000
#define KEY_SIZE 32    

// Server config
const char* ssid = "esp";
const char* password = "MyWifiZone123!";
WiFiServer server(5000);
WiFiClient persistentClient;
//const char* remoteIP = "192.168.244.201"; // red
const char* remoteIP = "192.168.244.196"; // white
const uint16_t remotePort = 5000;

// global variables
char unsigned  hash[HASH_SIZE] = { 0 };
String requested_file_name = "";
const uint8_t hash_code = 1;
const uint8_t data_code = 0;
const uint8_t msg_code = 47;
size_t nonces_table[MAX_NONCES] = {0};
unsigned char npub[NONCE_SIZE] = {0};
unsigned char current_nonce[NONCE_SIZE] = {0};
unsigned char key[KEY_SIZE] = {0};     //key


// Task handles
TaskHandle_t serverTaskHandle = NULL;
TaskHandle_t clientTaskHandle = NULL;

void print_nonce(const unsigned char* npub) {
    for (int i = 0; i < NONCE_SIZE; i++) {
        if (npub[i] < 0x10) Serial.print("0");
        Serial.print(npub[i], HEX);
    }
    Serial.println();
}

size_t nonce_to_integer(const unsigned char* npub){
  size_t nonce_value = 0;
  for (int i = 0; i < sizeof(size_t); i++) {
    nonce_value <<= 8;
    nonce_value |= npub[NONCE_SIZE - sizeof(size_t) + i];
  }
  return nonce_value;
}

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
  esp_task_wdt_deinit(); // watchdog cries without this. It believes funcitons get stuck when they do a lot of computing.
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
