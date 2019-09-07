#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "config.h"
#include "TheShed.h"

#define TRIGGER_PIN D0
#define ECHO_PIN D1

const char mqttServer[] = "192.168.4.20";
const char mqttPubTopic[] = "home/outside/watertank-status";
const char mqttSubTopic[] = "home/outside/watertank-control";
const char wifiHostname[] = "watertank-sensor";

/*
 * Example code
 * https://www.makerguides.com/jsn-sr04t-arduino-tutorial/
 */

TheShed* shedWifi;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();

  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);
  

  shedWifi = new TheShed(WIFI_SSID, WIFI_KEY, wifiHostname);
  shedWifi->setupMqtt(wifiHostname, mqttServer, 1883, mqttCallback, mqttSubTopic);
}

void loop() {
  ArduinoOTA.handle();
  shedWifi->mqttLoop();

  senseDistance();
  delay(100);
}

void senseDistance() {
  // Clear the TRIGGER_PIN by setting it LOW:
  digitalWrite(TRIGGER_PIN, LOW);
  
  delayMicroseconds(5);
 // Trigger the sensor by setting the TRIGGER_PIN high for 10 microseconds:
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(15);
  digitalWrite(TRIGGER_PIN, LOW);
  
  // Read the ECHO_PIN. pulseIn() returns the duration (length of the pulse) in microseconds
  // Datasheet indicates it could be up to 38ms if no obstable, which would be 38,000 microseconds.
  // Max (signed) int value is 32,767
  // Max unsigned int value is 65,535
  unsigned int duration = pulseIn(ECHO_PIN, HIGH);
  
  // Calculate the distance
  // Datasheet indicates max distance 600cm, well within bounds of (signed) int.
  int distance = duration*0.034/2;
  
  // Print the distance
  Serial.print("Distance = ");
  Serial.print(distance);
  Serial.println(" cm");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char *cstring = (char *) payload;
  Serial.println(cstring);

  // Deserialise JSON
  // https://arduinojson.org/v6/doc/deserialization/
  DynamicJsonDocument doc(100);
  DeserializationError err = deserializeJson(doc, cstring);

  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return;
  }

  const char* name = doc["command"];
  if (strcmp(name, "query") == 0) {
    Serial.println("Received query command");
    sendLevel();
  } else {
    Serial.println("Received unknown command"); 
  }
}

void sendLevel() {
  
  // Prepare payload
  char payload[50];
  int waterLevel;
  sprintf(payload, "{\"level\":%d}", waterLevel);

  Serial.println("Transmitting machine state:");
  Serial.println(payload);
  
  shedWifi->publishToMqtt(mqttPubTopic, payload);
}
