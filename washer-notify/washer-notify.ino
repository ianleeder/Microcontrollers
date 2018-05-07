#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EasyNTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"

#define SECOND 1000
#define QUARTER_SECOND 250
#define SENSOR_PIN D0

/*
 * GPIO spec info for a later date:
 * https://bbs.espressif.com/viewtopic.php?t=139
 * Max source current per pin: 12mA
 * Max source current total: 16x12mA (192mA)
 * Max sink current per pin: 20mA
 */

bool machineRunning = false;
bool lastState = false;
int lastTripped = 0;
int tripBucket = 0;
int tripBucketLastDripped = 0;
unsigned long machineStartTime;

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(SENSOR_PIN, INPUT);
  printMacAddress();
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
    machineStartTime = millis();
    Serial.println("Machine started");
  }

  delay(5);
}

void getTimeFromNtp(char* buf) {
  WiFiUDP udp;
  Serial.println("Fetching time...");
  EasyNTPClient ntpClient(udp, "pool.ntp.org", (11 * 60 * 60)); // AEDST

  unsigned long unixTime = ntpClient.getUnixTime();
  Serial.print("Unix Time (UTC+11): ");
  Serial.println(unixTime);

  Serial.printf("Local time (UTC+11):\t%d:%02d:%02d\n", getHours(unixTime), getMinutes(unixTime), getSeconds(unixTime));
  Serial.println();
  sprintf(buf, "%d:%02d", getHours(unixTime), getMinutes(unixTime));
}

void sendDoneNotification() {
  // Connect wifi
  Serial.println("Initialising wifi");
  WiFi.mode(WIFI_STA);
  WiFi.hostname("WasherNotify");
  WiFi.begin(WIFI_SSID, WIFI_KEY);

  while ((WiFi.status() != WL_CONNECTED)) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println();

  // Print some helpful debug info
  printWifiDetails();

  // Fetch the NTP time
  char time[10];
  getTimeFromNtp(time);

  unsigned long runTime = millis() - machineStartTime;
  int runMins = (int)(runTime / 1000 / 60);
  Serial.printf("Machine ran for %ums (%d mins)\n", runTime, runTime / 1000 / 60);

  // Use it to format a meaningful message
  char message[100];
  sprintf(message, "Washing machine finished at %s after running for %d minutes", time, runTime / 1000 / 60);

  // Send the notification via PushOver
  postToPushOver(String(message));

  // Finish
  WiFi.disconnect();
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

void printWifiDetails() {
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.println();
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
      if (https.available()) {
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

// Helpful time conversion functions
// https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html
inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}
