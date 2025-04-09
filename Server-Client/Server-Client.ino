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

const char* ssid = "esp";
const char* password = "";

// Server config
WiFiServer server(5000);

#define CRYPTO_BYTES 64
#define METADATA_HASH (sizeof(size_t) + 1 + CRYPTO_BYTES)
#define METADATA (sizeof(size_t) + 1) // This includes the size of the cyphertext chunk and a bit singaling if the hash should be read

//const char* remoteIP = "192.168.244.201"; // red
const char* remoteIP = "192.168.244.196"; // white
const uint16_t remotePort = 5000;

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

void send_file(String file_name){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  char unsigned finalhash[CRYPTO_BYTES] = { 0 };
  char unsigned compare[CRYPTO_BYTES] = { 0 };

  String encrypted_filed_name = file_name + ".ascon";
  Serial.printf("Encrypting file: %s\n", file_name);
  encrypt_file(file_name.c_str(), &SD, encrypted_filed_name.c_str());
  Serial.printf("Decrypting file: %s\n", encrypted_filed_name);
  decrypt_file(encrypted_filed_name.c_str(), &SD, "/decrypted.txt");

  hash_file(SD, "/decrypted.txt", compare);
  hash_file(SD, file_name.c_str(), finalhash);
  Serial.printf("Comparing hashes ...\n");
  print_hash_output(4, finalhash);
  print_hash_output(4, compare);

  listDir(SD, "/", 1);
}

int parse_input(String msg) {
  msg.trim();
  if (msg.startsWith("/GET")) {
    Serial.println("Command recognized: GET");
    String file_name = get_file_name(msg);
    if (file_name == "") {
      Serial.printf("File name could not be retrieved\n");
      return -1;
    }
    Serial.printf("File name is: %s\n", file_name);
    send_file(file_name);
    return 0;
  }

  Serial.println("Unrecognized command.");
  return -1;
}

// Server task
void serverTask(void* parameter) {
  server.begin();
  Serial.println("[Server] Started on port 5000");

  while (true) {
    WiFiClient client = server.available();
    if (client) {
      Serial.println("[Server] Client connected");
      while (client.connected()) {
        if (client.available()) {
          String msg = client.readStringUntil('\n');
          Serial.print("[Server] Received: ");
          Serial.println(msg);
          parse_input(msg);
        } else{
          delay(1);
        }
      }
      client.stop();
      Serial.println("[Server] Client disconnected");
    }
    delay(10); // avoid watchdog timeout
  }
}


WiFiClient persistentClient;

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
