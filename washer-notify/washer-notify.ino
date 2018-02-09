#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
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
  test();
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

  getFromIfttt();
  
  WiFi.disconnect();
}

void getFromIfttt() {
  HTTPClient http;
  http.begin(NOTIFY_URL);
  
  int httpCode = http.GET();
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
      Serial.printf("[HTTP] GET... failed, error code: %d\n", httpCode);
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void postToPushOver() {
  HTTPClient http;
  http.begin("https://api.pushover.net/1/messages.json");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("token=APP_TOKEN&user=USER_TOKEN&message=Washing+machine+finished");
  http.writeToStream(&Serial);
  http.end();
}

void testGet() {
  const char* serverName ="www.google.com";
  IPAddress serverIP;
  WiFi.hostByName(serverName, serverIP);

  Serial.printf("IP: %s\n", serverIP.toString().c_str());

  HTTPClient http;
  http.begin("http://www.google.com");
  
  int httpCode = http.GET();
  Serial.printf("First try: %d\n", httpCode);
  Serial.printf("First try: %s\n", http.errorToString(httpCode).c_str());
  httpCode = http.GET();
  Serial.printf("Second try: %d\n", httpCode);
  Serial.printf("Second try: %s\n", http.errorToString(httpCode).c_str());
}

void testHttps() {
  const char* certFingerprint = "73 11 35 12 67 DE 95 C6 A7 49 E6 64 43 9E 00 9F 10 56 2D 95";
  const char* url = "https://www.google.com.au";
  
  HTTPClient http;
  http.begin(url, certFingerprint);

  int httpCode = http.GET();
  Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
  if(httpCode > 0) {
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
      Serial.printf("[HTTPS] GET... failed, error code: %d\n", httpCode);
      Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void test() {
  // Connect Wifi
  Serial.println("Initialising wifi");
  WiFi.begin(WIFI_SSID, WIFI_KEY);
  
  while((WiFi.status() != WL_CONNECTED)) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  testHttps();
  
  WiFi.disconnect();
}
