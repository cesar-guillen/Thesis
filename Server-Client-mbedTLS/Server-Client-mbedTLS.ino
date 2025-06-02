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
size_t CHUNK_SIZE = 16;


// Server config
const char* ssid = "esp32";
const char* password = "MyWifiZone123!12";
WiFiServer server(5000);
WiFiClient persistentClient;
//const char* remoteIP = "192.168.128.201"; // red
const char* remoteIP = "192.168.128.196"; // white
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

void print_hex(const byte* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        if (i % 16 == 15) Serial.println();
        else Serial.print(" ");
    }
    Serial.println();
}

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
      Serial.printf("[Client] Connecting to remote... %s \n", remoteIP);
      if (persistentClient.connect(remoteIP, remotePort)) {
      } else {
        Serial.println("[Client] Failed to connect, retrying in 5 seconds...");
        delay(5000);
        continue;
      }
    }
    if (Serial.available()) { 
      String input = Serial.readStringUntil('\n');
      input.trim(); 
      send_request(input);
    }
    delay(100); 
  }
}

volatile bool keepMonitoring = true;

void monitor_task(void *param) {
  char stats[1024];
  while (keepMonitoring) {
    //vTaskGetRunTimeStats(stats);
    //Serial.println("CPU usage:\n" + String(stats));
    Serial.printf("Free heap: %d bytes\n\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    vTaskDelay(pdMS_TO_TICKS(100));  // Run every 500 ms
  }
  Serial.println("Monitoring ended.");
  vTaskDelete(NULL); // Cleanly delete this task
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // Allow time for Serial monitor to open

  for (size_t size = 8; size <= 4096; size *= 2) {
    Serial.printf("Allocating %u bytes...\n", size);

    void* ptr = malloc(size);
    if (ptr != nullptr) {
      unsigned long start = micros();
      memset(ptr, 0, size);
      unsigned long end = micros();

      Serial.printf("Zeroing %u bytes took %lu microseconds\n", size, end - start);
      free(ptr);
    } else {
      Serial.println("Failed to allocate memory.");
    }

    delay(1000);  // Delay between tests
  }
}



void loop() {
}