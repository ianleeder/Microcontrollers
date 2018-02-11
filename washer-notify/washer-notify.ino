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
  // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/HTTPSRequest/HTTPSRequest.ino
  const char* host = "api.pushover.net";
  const char* certFingerprint = "1E D5 B7 68 BB 25 AD A3 E0 96 78 A4 68 48 08 4F 07 E4 8D AB";
  String url = "/1/messages.json";
  WiFiClientSecure client;

  Serial.print("Connecting to ");
  Serial.println(host);

  if (! client.connect(host, 443)) {
    Serial.println("Connection failed. Halting execution.");
    while(1);
  }

  if (!client.verify(certFingerprint, host)) {
    Serial.println("Connection insecure! Halting execution.");
    while(1);
  }

  Serial.println("Connection secure.");

  String content = String("user=") + PUSHOVER_USER_TOKEN + "&" +
               "token=" + PUSHOVER_API_TOKEN + "&" +
               "message=Washing+done!\r\n";

  int len = content.length();
  Serial.println("Length: ");
  Serial.println(len);
  
  String post = String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               //"User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n" + 
               "Cache-Control: no-cache\r\n" +
               "Postman-Token: ac310ab9-9cc5-89fe-ad8f-5194e085e5a9\r\n\r\n" +
               "user=" + PUSHOVER_USER_TOKEN + "&" +
               "token=" + PUSHOVER_API_TOKEN + "&" +
               "message=Washing+done!\r\n\r\n";// +
               //"Connection: close\r\n\r\n";

  

  Serial.println("Sending:");
  Serial.println(post);

  client.print(post);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
}

void postToPushOver2(String message) {
  // http://blog.discoverthat.co.uk/2017/05/use-pushover-notification-with-esp8266.html
  
  // Pushover requires encrypted messages when sending to groups or anyone other
  // than the owner of the app
  WiFiClientSecure https;
  // Form the string
  String parameters = String("token=") + PUSHOVER_API_TOKEN + "&user=" + PUSHOVER_USER_TOKEN + "&message=" + message;
  int length = parameters.length();
  if (https.connect("api.pushover.net", 443)) {
    Serial.println("Start posting notification: " + parameters);
    https.println("POST /1/messages.json HTTP/1.1");
    https.println("Host: api.pushover.net");
    https.println("Connection: close\r\nContent-Type: application/x-www-form-urlencoded");
    https.print("Content-Length: ");
    https.print(length);
    https.println("\r\n");
    https.print(parameters);
    // ==
    // Reply from the server:
    while(https.connected()) {
      while(https.available()) {
        char ch = https.read();
        Serial.write(ch);
      }
    }
    // ==
    https.stop();
    Serial.println("Finished posting notification.");
  }
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

void testHttps2() {
  const char* host = "www.google.com.au";
  const char* certFingerprint = "73 11 35 12 67 DE 95 C6 A7 49 E6 64 43 9E 00 9F 10 56 2D 95";
  WiFiClientSecure client;

  Serial.print("Connecting to ");
  Serial.println(host);
  Serial.println(certFingerprint);

  if (! client.connect(host, 443)) {
    Serial.println("Connection failed. Halting execution.");
    while(1);
  }

  if (client.verify(certFingerprint, host)) {
    Serial.println("Connection secure.");
  } else {
    Serial.println("Connection insecure! Halting execution.");
    while(1);
  }
}

void testHttps3() {
  const char* host = "io.adafruit.com";
  const char* certFingerprint = "77 00 54 2D DA E7 D8 03 27 31 23 99 EB 27 DB CB A5 4C 57 18";
  WiFiClientSecure client;

  Serial.print("Connecting to ");
  Serial.println(host);

  if (! client.connect(host, 443)) {
    Serial.println("Connection failed. Halting execution.");
    while(1);
  }

  if (client.verify(certFingerprint, host)) {
    Serial.println("Connection secure.");
  } else {
    Serial.println("Connection insecure! Halting execution.");
    while(1);
  }
}

void testHttps4() {
  const char* host = "api.pushover.net";
  const char* certFingerprint = "1E D5 B7 68 BB 25 AD A3 E0 96 78 A4 68 48 08 4F 07 E4 8D AB";
  WiFiClientSecure client;

  Serial.print("Connecting to ");
  Serial.println(host);

  if (! client.connect(host, 443)) {
    Serial.println("Connection failed. Halting execution.");
    while(1);
  }

  if (!client.verify(certFingerprint, host)) {
    Serial.println("Connection insecure! Halting execution.");
    while(1);
  }

  Serial.println("Connection secure.");


  
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
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  postToPushOver2("Test from ESP8266");
  
  WiFi.disconnect();
}
