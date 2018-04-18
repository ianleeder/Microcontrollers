#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EasyNTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"

#define SECOND 1000

#define OPEN_PIN D0
#define CLOSE_PIN D1
#define ACTIVATE_PIN D2

#define MQTT_SERVER "192.168.0.20"
#define MQTT_TOPIC "gdoor"

/*
 * Door states
 * 0 - Unknown
 * 1 - Open
 * 2 - Closed
 * 3 - Opening
 * 4 - Closing
 */
byte previousDoorState = 0;
byte doorState = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  printMacAddress();

  // Connect wifi
  Serial.println("Initialising wifi");
  WiFi.hostname("gdoor-sensor");
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
  
}

void loop() {
  // put your main code here, to run repeatedly:

}

bool checkPin(byte pin) {
  
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
