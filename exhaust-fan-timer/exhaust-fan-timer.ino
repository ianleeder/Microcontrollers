#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "config.h"
#include "TheShed.h"

const byte relayPin = D0;

const char mqttServer[] = "192.168.4.20";
const char mqttPubTopic[] = "home/main-toilet/fan-status";
const char mqttSubTopic[] = "home/main-toilet/fan-control";
const char wifiHostname[] = "main-toilet-fan";

const int defaultFanDuration = 900 * 1000; // 15 minutes
const int pollPeriod = 100; // Keep it short so we don't interfere with OTA update poll

TheShed* shedWifi;

bool fanState = false;
unsigned long fanStartTime;
int fanDuration;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();

  shedWifi = new TheShed(WIFI_SSID, WIFI_KEY, wifiHostname);
  shedWifi->setupMqtt(wifiHostname, mqttServer, 1883, mqttCallback, mqttSubTopic);

  // Set up pins
  pinMode(relayPin, OUTPUT);

  // Turn the fan on
  setFan(true);
}

void loop() {
  ArduinoOTA.handle();
  shedWifi->mqttLoop();

  if(fanState && (millis() - fanStartTime > fanDuration)) {
    setFan(false);
  }

  delay(pollPeriod);
}

void setFan(bool state) {
  setFan(state, defaultFanDuration);
}

void setFan(bool state, int duration) {
  fanState = state;
  digitalWrite(relayPin, state ? HIGH : LOW);

  if(state) {
    fanDuration = duration;
    fanStartTime = millis();
  }

  sendFanState();
}

void sendFanState() {
  Serial.print("Transmitting fan state: ");
  Serial.print(fanState);
  Serial.print(" (");
  Serial.print(fanState?"on":"off");
  Serial.println(")");

  // Send payload
  char payload[60];
  sprintf(payload, "{\"state\":%s, \"description\":\"%s\", \"duration\":%d}", fanState?"true":"false", fanState?"on":"off", fanDuration/1000);
  
  shedWifi->publishToMqtt(mqttPubTopic, payload);
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
    sendFanState();
  }
}
