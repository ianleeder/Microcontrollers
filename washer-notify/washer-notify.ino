#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

#define NOTIFY_URL "http://www.google.com/index.html"

#define SECOND 1000
#define QUARTER_SECOND 250

#define SENSOR_PIN D0

bool machineRunning = false;

bool lastState = false;
int lastTripped = 0;

int tripBucket = 0;
int tripBucketLastDripped = 0;


void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(SENSOR_PIN, INPUT);
  printMacAddress();
  sendDoneNotification();
}

void printMacAddress() {
  // Code to get the unit MAC address
  // https://techtutorialsx.com/2017/04/09/esp8266-get-mac-address/
  // EC:FA:BC:8B:C1:8F
  Serial.println();
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println();
}


void loop() {
  int now = millis();
  int sinceLastTripped = now - lastTripped;
  int sinceLastDrip = now - tripBucketLastDripped;

  if (tripBucket > 0 && sinceLastDrip > SECOND) {
    tripBucket--;
    tripBucketLastDripped = now;
    Serial.print("Drip! ");
    Serial.println(tripBucket);
  }

  // Read the state and see if the sensor was tripped
  bool state = digitalRead(SENSOR_PIN) == 0 ? false : true;
  if (lastState != state) {
    lastState = state;

    // Can be tripped a maximum of once per second
    if (sinceLastTripped > QUARTER_SECOND) {
      lastTripped = now;

      if (tripBucket < 300) {
        tripBucket++;
      }
    }
  }

  if (machineRunning && tripBucket == 0) {
    machineRunning = false;
    Serial.println("Machine stopped");
    sendDoneNotification();
  }

  if (!machineRunning && tripBucket > 60) {
    machineRunning = true;
    Serial.println("Machine started");
  }

  delay(5);
}


void sendDoneNotification() {
  // Connect wifi
  Serial.println("Initialising wifi");
  WiFi.hostname("WasherNotify");
  WiFi.begin(WIFI_SSID, WIFI_KEY);
  
  while((WiFi.status() != WL_CONNECTED)) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  postToPushOver("Done");
  
  WiFi.disconnect();
}

void postToPushOver(String message) {
  // http://blog.discoverthat.co.uk/2017/05/use-pushover-notification-with-esp8266.html
  
  // Pushover requires encrypted messages when sending to groups or anyone other
  // than the owner of the app
  
  WiFiClientSecure https;
  // Form the string
  String parameters = String("token=") + PUSHOVER_API_TOKEN + "&user=" + PUSHOVER_USER_TOKEN + "&message=" + message;
  int length = parameters.length();

  Serial.println("Starting post");
  if (https.connect("api.pushover.net", 443)) {
    https.println("POST /1/messages.json HTTP/1.1");
    https.println("Host: api.pushover.net");
    https.println("Connection: close\r\nContent-Type: application/x-www-form-urlencoded");
    https.print("Content-Length: ");
    https.print(length);
    https.println("\r\n");
    https.print(parameters);

    while (https.connected()) {
      // Proceed if data is available for reading
      if(https.available()) {
        String line = https.readStringUntil('\n');
        //Serial.println("Header: " + line);
        
        // If we read a blank line, that is the end of the headers
        if (line == "\r") {
          //Serial.println("headers received");
          break;
        }
      }
    }
    
    https.readStringUntil('\n');
    String jsonResponse = https.readStringUntil('\n');

    Serial.println("JSON data received: " + jsonResponse);

    /*
    for(char& c : jsonResponse) {
      Serial.printf("%c (%u)\n",c, c);
    }
    */
    // Parse the JSON
    // https://techtutorialsx.com/2016/07/30/esp8266-parsing-json/amp/
    // Ensure we allocate plenty of space in the buffer
    // If we are parsing a read-only string, it needs to copy parts
    // https://arduinojson.org/api/jsonbuffer/parse/#description
    // Otherwise we spend an hour wondering why the string didn't parse, when it
    // was just the buffer size too small.
    StaticJsonBuffer<200> JSONBuffer;
    JsonObject& root = JSONBuffer.parseObject(jsonResponse);

    if (!root.success()) {
      Serial.println("Parsing failed");
    } else {
      Serial.printf("Status: %d\n", root["status"].as<int>());
      Serial.printf("Request: %s\n", root["request"].as<char*>());
    }
    
    https.stop();
    Serial.println("Finished posting notification.");
  }
}
