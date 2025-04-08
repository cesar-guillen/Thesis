#include <WiFi.h>

const char* ssid = "esp";
const char* password = "MyWifiZone123!";

WiFiServer server(5000);

// Client config
const char* remoteIP = "192.168.244.196"; //other client's IP
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
  return secondWord;
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
    return 0;
  }

  Serial.println("Unrecognized command.");
  return -1;
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
    delay(100); 
  }
}

// Client task
// Global so we can reuse across loop
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
      input.trim(); 

      if (input.length() > 0) {
        Serial.print("[Client] Sending message: ");
        Serial.println(input);
        persistentClient.println(input);  // Send message
      }
    }
    delay(100);
  }
}


void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  // Start tasks on differnt cores
  xTaskCreatePinnedToCore(serverTask,"Server Task",8192,NULL,1,&serverTaskHandle,0);
  xTaskCreatePinnedToCore(clientTask,"Client Task",8192,NULL,1,&clientTaskHandle,1);
}

void loop() {
}
