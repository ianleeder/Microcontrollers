#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EasyNTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"

#define SECOND 1000

/*
 * Pin mapping image:
 * https://arduino.stackexchange.com/q/25260
 * 
 * Pin mapping text:
 * https://github.com/esp8266/Arduino/issues/584
    static const uint8_t D0   = 16;
    static const uint8_t D1   = 5;
    static const uint8_t D2   = 4;
    static const uint8_t D3   = 0;
    static const uint8_t D4   = 2;
    static const uint8_t D5   = 14;
    static const uint8_t D6   = 12;
    static const uint8_t D7   = 13;
    static const uint8_t D8   = 15;
    static const uint8_t D9   = 3;
    static const uint8_t D10  = 1;
 * 
 * Notes in pins supporting interrupts:
 * http://www.esp8266.com/wiki/doku.php?id=esp8266_gpio_pin_allocations
 * Pin interrupts are supported through attachInterrupt(), detachInterrupt() functions.
 * Interrupts may be attached to any GPIO pin except GPIO16, but since GPIO6-GPIO11 are
 * typically used to interface with the flash memory ICs on most esp8266 modules,
 * applying interrupts to these pins are likely to cause problems.
 * 
 * Basically don't use D0 for input interrupts.
 * 
 * Good tutorial on external interrupt pins
 * https://techtutorialsx.com/2016/12/11/esp8266-external-interrupts/
 * 
 * Door states
 * 0 - Unknown
 * 1 - Open
 * 2 - Closed
 * 3 - Opening
 * 4 - Closing
 */

const byte activatePin = D0;
const byte openPin = D1;
const byte closePin = D2;

const char mqttServer[] = "192.168.4.20";
const char mqttTopic[] = "gdoor";

volatile byte previousDoorState = 0;
volatile byte doorState = 0;
volatile bool publishChange = false;

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

  // Set up pins
  pinMode(openPin, INPUT_PULLUP);
  pinMode(closePin, INPUT_PULLUP);
  pinMode(activatePin, OUTPUT);

  /*
   * 
   * Input pins are pull-up, which means they will be high unless "active"
   * When door is closed        openPin = 1, closePin = 0
   * When door starts opening   openPin = 1, closePin = 0->1 (closePin rising)
   * When door completes open   openPin = 1->0, closePin = 1 (openPin falling)
   * When door is open          openPin = 0, closePin = 1
   * When door starts closing   openPin = 0->1, closePin = 1 (openPin rising)
   * When door completes close  openPin = 1, closePin = 1->0 (closePin falling)
   */

  // Can't attach two interrupts (one rising, one falling) to the same pin :(
  // https://forum.arduino.cc/index.php?topic=147825.0
  attachInterrupt(digitalPinToInterrupt(closePin), closeChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(openPin), openChange, CHANGE);
}

void loop() {
  if(publishChange) {
    Serial.print("Door state changed: ");
    Serial.println(doorState);
    publishChange = false;
  }
}

void closeChange() {
  int pinState = digitalRead(closePin);
  // If pin is 1 then it was a rising transition
  // If it has a value (1) then it will evaluate to true
  // If closePin rising then state is opening (3), else closed (2)
  int newState = pinState ? 3 : 2;

  // By checking if doorState != new state we do rudimentary debouncing
  // and don't publish the same message multiple times in a row
  if(doorState != newState) {
    previousDoorState = doorState;
    doorState = newState;
    publishChange = true;
  }
}

void openChange() {
  int pinState = digitalRead(openPin);
  // If pin is 1 then it was a rising transition
  // If it has a value (1) then it will evaluate to true
  // If openPin rising then state is closing (4), else open (1)
  int newState = pinState ? 4 : 1;

  // By checking if doorState != new state we do rudimentary debouncing
  // and don't publish the same message multiple times in a row
  if(doorState != newState) {
    previousDoorState = doorState;
    doorState = newState;
    publishChange = true;
  }
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
