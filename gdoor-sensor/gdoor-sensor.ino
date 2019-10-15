#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "config.h"
#include "TheShed.h"

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
 */

const byte DOOR_OPEN = 0;
const byte DOOR_CLOSED = 1;
const byte DOOR_OPENING = 2;
const byte DOOR_CLOSING = 3;
const byte DOOR_ALERT = 4;
const byte DOOR_UNKNOWN = 5;

const char* doorStates[] = {"Open", "Closed", "Opening", "Closing", "ALERT OPEN", "Unknown"};

const byte activatePin = D0;
const byte openPin = D1;
const byte closePin = D2;

const char mqttServer[] = "192.168.4.20";
const char mqttPubTopic[] = "home/garage/door-status";
const char mqttSubTopic[] = "home/garage/door-control";
const char wifiHostname[] = "gdoor-sensor";

const int doorMovingTimeout = 60 * 1000;    // 1 minute
const int doorOpenTimeout = 15 * 60 * 1000; // 15 minutes
const int doorButtonPressTime = 500;        // 500ms
const int pollPeriod = 100; // Keep it short so we don't interfere with OTA update poll

byte doorState = DOOR_UNKNOWN;
unsigned long lastDoorMoveTime = millis();

int closePinState = 1;
int openPinState = 1;

TheShed* shedWifi;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();

  shedWifi = new TheShed(WIFI_SSID, WIFI_KEY, wifiHostname);
  shedWifi->setupMqtt(wifiHostname, mqttServer, 1883, mqttCallback, mqttSubTopic);

  // Set up pins
  pinMode(openPin, INPUT_PULLUP);
  pinMode(closePin, INPUT_PULLUP);
  pinMode(activatePin, OUTPUT);
  digitalWrite(activatePin, HIGH);

  // Read initial state
  openPinState = digitalRead(openPin);
  closePinState = digitalRead(closePin);
  // One of these pins needs to be "closed" (0)
  // Inputs are active-low (with pullup resistors)
  // A zero is boolean false, so !oPin means the openPin is currently "active" (and the door is open)
  if(!openPinState) {
    doorState = DOOR_OPEN;
  } else if (!closePinState) {
    doorState = DOOR_CLOSED;
  } else {
    doorState = DOOR_UNKNOWN;
  }

  Serial.print("Start up door state: ");
  Serial.println(doorStates[doorState]);
  /*
  char payload[50];
  sprintf(payload, "init - closePin: %d   openPin: %d", closePinState, openPinState);
  shedWifi->publishToMqtt("home/garage/debug", payload);
  */
}

 /*
  * Input pins are pull-up, which means they will be high unless "active"
  * When door is closed        openPin = 1, closePin = 0
  * When door starts opening   openPin = 1, closePin = 0->1 (closePin rising)
  * When door completes open   openPin = 1->0, closePin = 1 (openPin falling)
  * When door is open          openPin = 0, closePin = 1
  * When door starts closing   openPin = 0->1, closePin = 1 (openPin rising)
  * When door completes close  openPin = 1, closePin = 1->0 (closePin falling)
  */

void loop() {
  ArduinoOTA.handle();
  shedWifi->mqttLoop();

  /*
   * Even with the 10K pullup resistors, the fluoro lights switching on and off introduce enough
   * noise on the lines to trigger the interrupt routines (and false positives).
   * 
   * We can't use interrupts :(
   * https://github.com/esp8266/Arduino/issues/1372
   * 
   * Instead let's poll periodically.
   */

  int newClosePinState = digitalRead(closePin);
  int newOpenPinState = digitalRead(openPin);

  // If we detect a change in pin state
  if(newOpenPinState != openPinState || newClosePinState != closePinState) {
    
    /*
    shedWifi->publishToMqtt("home/garage/debug", "Detected pin change");
    int consistentReads = 0;
    do {
      // Check a few times to ensure we have a consistent reading on the pins
      delay(consistentReadDelay);
      int tempClosePinState = digitalRead(closePin);
      int tempOpenPinState = digitalRead(openPin);

      if(tempClosePinState == newClosePinState && tempOpenPinState == newOpenPinState) {
        consistentReads++;
      } else {
        newClosePinState = tempClosePinState;
        newOpenPinState = tempOpenPinState;
        consistentReads = 0;
      }
    } while(consistentReads < consistentReadsRequired); 
    
    // We have a consistent idea of the pin state, now adjust door state
    char payload[50];
    sprintf(payload, "change - closePin: %d   openPin: %d", newClosePinState, newOpenPinState);
    shedWifi->publishToMqtt("home/garage/debug", payload);
    */
    
    // If the pins haven't actually changed state, we just had a false positive pin change
    if(openPinState == newOpenPinState && closePinState == newClosePinState) {
      return;
    }
    // if the open pin changed but close pin did not
    else if(openPinState != newOpenPinState && closePinState == newClosePinState) {
      // If pin is 1 then it was a rising transition
      // If it has a value (1) then it will evaluate to true
      // If openPin rising then state is closing, else open
      doorState = newOpenPinState ? DOOR_CLOSING : DOOR_OPEN;
      lastDoorMoveTime = millis();
      sendDoorState();
    }
    // Or if the close pin changed but open pin did not
    else if (closePinState != newClosePinState && openPinState == newOpenPinState) {
      // If pin is 1 then it was a rising transition
      // If it has a value (1) then it will evaluate to true
      // If closePin rising then state is opening, else closed
      doorState = newClosePinState ? DOOR_OPENING : DOOR_CLOSED;
      lastDoorMoveTime = millis();
      sendDoorState();
    }
    // Otherwise two pins changed and we are unknown
    else {
      doorState = DOOR_UNKNOWN;
      sendDoorState();
    }
    
    openPinState = newOpenPinState;
    closePinState = newClosePinState;
  }
 
   // If the door was moving, and the time elapsed is greater than our timeout
  if((doorState == DOOR_OPENING || doorState == DOOR_CLOSING) && (millis() - lastDoorMoveTime > doorMovingTimeout)) {
    Serial.print("Door did not finish moving, last seen it was ");
    Serial.println(doorStates[doorState]);
    Serial.println("Changing state to unknown");
    doorState = DOOR_UNKNOWN;
    sendDoorState();
  }

  // If the door is open or unknown, and the time elapsed is greater than our timeout
  if((doorState == DOOR_OPEN || doorState == DOOR_UNKNOWN) && (millis() - lastDoorMoveTime > doorOpenTimeout)) {
    Serial.print("ALERT -- Door is left open");
    doorState = DOOR_ALERT;
    sendDoorState();
  }

  delay(pollPeriod);
}

void pulseDoor() {
  digitalWrite(activatePin, LOW);
  delay(doorButtonPressTime);
  digitalWrite(activatePin, HIGH);
}

void sendDoorState() {
  Serial.print("Transmitting door state: ");
  Serial.print(doorState);
  Serial.print(" (");
  Serial.print(doorStates[doorState]);
  Serial.println(")");

  // Send payload
  char payload[50];
  sprintf(payload, "{\"state\":%d, \"description\":\"%s\"}", doorState, doorStates[doorState]);
  
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
    sendDoorState();
  } else if (strcmp(name,"close") == 0) {
    Serial.println("Received close command");
    // Don't act on command if door is closed, closing or opening.
    if(doorState == DOOR_OPEN || doorState == DOOR_ALERT || doorState == DOOR_UNKNOWN) {
      pulseDoor();
    }
  } else if (strcmp(name,"open") == 0) {
    Serial.println("Received open command");
    // Don't act on command if door is open, opening or closing.
    if(doorState == DOOR_CLOSED || doorState == DOOR_ALERT || doorState == DOOR_UNKNOWN) {
      pulseDoor();
    }
  }
  // DEBUG COMMAND
  else if (strcmp(name,"pulse") == 0) {
    Serial.println("Received pulse command");
    pulseDoor();
  } else {
    Serial.println("Received unknown command"); 
  }
}
