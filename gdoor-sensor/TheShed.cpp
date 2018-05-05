#include "Arduino.h"
#include "TheShed.h"
#include <ESP8266WiFi.h>
#include <EasyNTPClient.h>
#include <WiFiUdp.h>

TheShed::TheShed(const char ssid[], const char key[], const char host[])
{
  strcpy(_key, key);
  strcpy(_ssid, ssid);
 
  if(strlen(host)>0) {
    Serial.print("Setting hostname: ");
    Serial.println(host);
    WiFi.hostname(host);
  }
}

void TheShed::test() {
  
}

void TheShed::connectWifi() {
  printMacAddress();
  
  // Connect wifi
  Serial.println("Initialising wifi");
  WiFi.begin(_ssid, _key);

  int i = 0;
  while ((WiFi.status() != WL_CONNECTED)) {
    delay(100);
    Serial.print(".");
    if (++i == 100) {
      i = 0;
      Serial.println();
    }
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println();

  // Print some helpful debug info
  printWifiDetails();
}

void TheShed::disconnectWifi() {
  
}

void TheShed::printMacAddress() {
  // Code to get the unit MAC address
  // https://techtutorialsx.com/2017/04/09/esp8266-get-mac-address/
  // EC:FA:BC:8B:C1:8F
  Serial.println();
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println();
}

void TheShed::printWifiDetails() {
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.println();
}

void TheShed::getTimeFromNtp(char* buf) {
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
inline int TheShed::getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int TheShed::getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int TheShed::getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}
