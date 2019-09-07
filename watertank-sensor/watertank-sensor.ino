#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "config.h"
#include "TheShed.h"

const char mqttServer[] = "192.168.4.20";
const char mqttPubTopic[] = "home/outside/watertank-status";
const char mqttSubTopic[] = "home/outside/watertank-control";
const char wifiHostname[] = "watertank-sensor";

unsigned int waterLevel;
TheShed* shedWifi;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();

  shedWifi = new TheShed(WIFI_SSID, WIFI_KEY, wifiHostname);
  shedWifi->setupMqtt(wifiHostname, mqttServer, 1883, mqttCallback, mqttSubTopic);
}

void loop() {
  ArduinoOTA.handle();
  shedWifi->mqttLoop();
  
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
  sprintf(payload, "{\"level\":%d}", waterLevel);

  Serial.println("Transmitting machine state:");
  Serial.println(payload);
  
  shedWifi->publishToMqtt(mqttPubTopic, payload);
}
