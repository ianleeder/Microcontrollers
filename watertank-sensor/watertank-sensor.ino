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

// This is how much water the tank holds
const int TankCapacity = 2000;

// This is the distance from sensor to the water surface when full
const int DistanceTankFull = 30;

// This is the distance from sensor to the bottom of the tank when empty
const int DistanceTankEmpty = 150;

// This is how many times we can retry for a valid distance reading before failing
const int ValidationRetryLimit = 10;

// Allow for min - 5 or max + 5 on sensor readings.
const int SensorErrorMargin = 5;

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
  pinMode(ECHO_PIN, INPUT_PULLUP);
  

  shedWifi = new TheShed(WIFI_SSID, WIFI_KEY, wifiHostname);
  shedWifi->setupMqtt(wifiHostname, mqttServer, 1883, mqttCallback, mqttSubTopic);
}

void loop() {
  ArduinoOTA.handle();
  shedWifi->mqttLoop();
}

// Returns distance in centimeters
// Raw reading from sensor
int senseDistance() {
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
  Serial.print("Raw distance = ");
  Serial.print(distance);
  Serial.println(" cm");

  return distance;
}

// Returns distance in centimetres, or -1 if could not get a stable reading from ultrasonic sensor
int getValidatedDistance() {
  int distance = 0;
  int retries = 0;
  do {
    if(retries++ >= ValidationRetryLimit)
      return -1;
    
    distance = senseDistance();

    // Allow for small discrepancy in reading min/max
    if(distance > DistanceTankEmpty && distance <= (DistanceTankEmpty + SensorErrorMargin))
      distance = DistanceTankEmpty;

    if(distance < DistanceTankFull && distance >= (DistanceTankFull - SensorErrorMargin))
      distance = DistanceTankFull;
    
  } while (distance < DistanceTankFull || distance > DistanceTankEmpty);

  // Print the distance
  Serial.print("Validated distance = ");
  Serial.print(distance);
  Serial.println(" cm");

  return distance;
}

// Returns tank volume in litres, or -1 if could not get a stable reading from ultrasonic sensor
int getWaterTankLevel() {
  // Distance is cm from sensor.
  int distance = getValidatedDistance();

  // If we could not get a valid distance, don't attempt to return a valid level
  if(distance == -1)
    return -1;

  // Need to convert this into litres.
  // For now assume a linear relationship between distance and
  // water volume (hint this is wrong, the tank is not a regular shape but it's too hard and not important).

  // 20cm = full, 2000ltr
  // 150cm = empty, 0ltr
  // (150-20/2)+20cm = 85cm = half, 1000ltr

  // Range = max dist (empty) - min dist (full) = 130cm
  // Scale factor = (max dist - distance) / range
  //              = (empty - distance) / (empty - full)
  // Volume = capacity * scale factor
  //        = 2000L * (empty - distance) / (empty - full)

  // So if distance is 20cm (eg full), we get scaling factor:
  // 150-20/150-20 = 1
  // If distance is 150cm (eg empty):
  // 150-150/150-20 = 0
  // And if distance is 85cm (eg half):
  // 150-85/150-20 = 65/130 = 0.5

  return TankCapacity * (DistanceTankEmpty - distance) / (DistanceTankEmpty - DistanceTankFull);
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
  int waterLevel = getWaterTankLevel();
  sprintf(payload, "{\"level\":%d}", waterLevel);

  Serial.println("Transmitting tank level:");
  Serial.println(payload);
  
  shedWifi->publishToMqtt(mqttPubTopic, payload);
}
