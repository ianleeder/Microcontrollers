#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "config.h"
#include "TheShed.h"

/*
 * Project based upon this tutorial
 * https://blog.erindachtler.me/tutorial-a-wifi-enabled-washing-machine/
 */

#define DECREMENT_PERIOD 1000
#define INCREMENT_PERIOD 500
#define SENSOR_PIN D0

// If we have consistent movement for more than 60 seconds,
// this indicates the machine has started.
#define START_ACTIVITY_THRESHOLD 60

// Limit how much activity we store on the counter.
// Since the counter is decremented every second (DECREMENT_PERIOD)
// this essentially means "What is our quiet period?".
// If we see 300s with no activity, our counter will fall to zero and
// the machine is done.
#define MAX_ACTIVITY_COUNT 300

/*
 * GPIO spec info for a later date:
 * https://bbs.espressif.com/viewtopic.php?t=139
 * Max source current per pin: 12mA
 * Max source current total: 16x12mA (192mA)
 * Max sink current per pin: 20mA
 */

const char mqttServer[] = "192.168.4.20";
const char mqttPubTopic[] = "home/laundry/washer-status";
const char mqttSubTopic[] = "home/laundry/washer-control";
const char wifiHostname[] = "washer-sensor";

bool machineRunning = false;
bool lastMotionState = false;
int lastMotion = 0;
int lastMotionDecrement = 0;
int motionCounter = 0;
unsigned long machineStartTime;

TheShed* shedWifi;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();

  shedWifi = new TheShed(WIFI_SSID, WIFI_KEY, wifiHostname);
  shedWifi->setupMqtt(wifiHostname, mqttServer, 1883, mqttCallback, mqttSubTopic);

  pinMode(SENSOR_PIN, INPUT);
}

void loop() {
  ArduinoOTA.handle();
  shedWifi->mqttLoop();
  
  int now = millis();
  // Check how long since we last checked for motion
  // and how long since we last detected motion
  int sinceLastMotionDecrement = now - lastMotionDecrement;
  int sincelastMotion = now - lastMotion;

  // Decrement our counter every second
  if (motionCounter > 0 && sinceLastMotionDecrement > DECREMENT_PERIOD) {
    motionCounter--;
    lastMotionDecrement = now;
    Serial.print("Motion Counter = ");
    Serial.println(motionCounter);
  }

  // Read the state and see if the sensor was tripped
  bool currentMotionState = digitalRead(SENSOR_PIN);
  if (lastMotionState != currentMotionState) {
    lastMotionState = currentMotionState;

    // Can be tripped a maximum of twice per second (once every 500ms)
    if (sincelastMotion > INCREMENT_PERIOD) {
      lastMotion = now;
      if (motionCounter < MAX_ACTIVITY_COUNT) {
        motionCounter++;
      }
    }
  }

  // If it was running but our counter is down to zero, stop
  if (machineRunning && motionCounter == 0) {
    machineRunning = false;
    Serial.println("Machine stopped");
    sendMachineState(true);
  }

  // If machine is not running and our motion counter is above the threshold
  if (!machineRunning && motionCounter > START_ACTIVITY_THRESHOLD) {
    machineRunning = true;
    machineStartTime = millis();
    Serial.println("Machine started");
    sendMachineState(false);
  }

  delay(5);
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
    sendMachineState(false);
  } else {
    Serial.println("Received unknown command"); 
  }
}

void sendMachineState(bool finished) {
  unsigned long runTime = millis() - machineStartTime;
  int runMins = (int)(runTime / 1000 / 60);

  if(finished)
    Serial.printf("Machine ran for %ums (%d mins)\n", runTime, runTime / 1000 / 60);
  
  // Prepare payload
  char payload[100];
  sprintf(payload, "{\"finished\":%s, \"running\":%s, \"runTime\":%d, \"motionCounter\":%d}", finished?"true":"false", machineRunning?"true":"false", machineRunning?runTime:0, motionCounter);

  Serial.println("Transmitting machine state:");
  Serial.println(payload);
  
  shedWifi->publishToMqtt(mqttPubTopic, payload);
}
