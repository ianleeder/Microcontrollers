#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
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
 * 
 * Door states
 * 0 - Unknown
 * 1 - Open
 * 2 - Closed
 * 3 - Opening
 * 4 - Closing
 * 5 - Alert open
 */

const byte DOOR_UNKNOWN = 0;
const byte DOOR_OPEN = 1;
const byte DOOR_CLOSED = 2;
const byte DOOR_OPENING = 3;
const byte DOOR_CLOSING = 4;
const byte DOOR_ALERT = 5;

const char* doorStates[] = {"Unknown", "Open", "Closed", "Opening", "Closing", "ALERT OPEN"};

const byte activatePin = D0;
const byte openPin = D1;
const byte closePin = D2;

const char mqttServer[] = "192.168.4.20";
const char mqttPubTopic[] = "home/garage/door-status";
const char mqttSubTopic[] = "home/garage/door-query";
const char wifiHostname[] = "gdoor-sensor";

const int doorMovingTimeout = 60 * 1000;    // 1 minute
const int doorOpenTimeout = 15 * 60 * 1000; // 15 minutes
const int debounceTime = 300;               // 300ms
const int doorButtonPressTime = 500;        // 500ms

byte doorState = DOOR_UNKNOWN;
volatile bool closePinTriggered = false;
volatile bool openPinTriggered = false;
unsigned long lastDoorMoveTime = millis();

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

  // Can't attach two interrupts (one rising, one falling) to the same pin :(
  // https://forum.arduino.cc/index.php?topic=147825.0
  attachInterrupt(digitalPinToInterrupt(closePin), closeChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(openPin), openChange, CHANGE);

  // Read initial state
  int oPin = digitalRead(openPin);
  int cPin = digitalRead(closePin);
  // One of these pins needs to be "closed" (0)
  // A zero is boolean false, so !oPin means if the openPin is currently "active" (actually open)
  if(!oPin) {
    doorState = DOOR_OPEN;
  } else if (!cPin) {
    doorState = DOOR_CLOSED;
  } else {
    doorState = DOOR_UNKNOWN;
  }

  Serial.print("Start up door state: ");
  Serial.println(doorStates[doorState]);
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

  if(closePinTriggered) {
    // Debounce delay
    delay(debounceTime);
    
    // Now contact has settled read current state
    int pinState = digitalRead(closePin);
    
    // If pin is 1 then it was a rising transition
    // If it has a value (1) then it will evaluate to true
    // If closePin rising then state is opening, else closed
    doorState = pinState ? DOOR_OPENING : DOOR_CLOSED;
    closePinTriggered = false;
    lastDoorMoveTime = millis();
    sendDoorState();
  }

  if(openPinTriggered) {
    // Debounce delay
    delay(debounceTime);
    
    // Now contact has settled read current state
    int pinState = digitalRead(openPin);
    
    // If pin is 1 then it was a rising transition
    // If it has a value (1) then it will evaluate to true
    // If openPin rising then state is closing, else open
    doorState = pinState ? DOOR_CLOSING : DOOR_OPEN;
    openPinTriggered = false;
    sendDoorState();
  }
 
   // If the door was moving, and the time elapsed is greater than our timeout
  if((doorState == DOOR_OPENING || doorState == DOOR_CLOSING) && (millis() - lastDoorMoveTime > doorMovingTimeout)) {
    Serial.print("Door did not finish moving, last seen it was ");
    Serial.println(doorStates[doorState]);
    Serial.println("Changing state to unknown");
    doorState = 0;
    sendDoorState();
  }

  // If the door is open or unknown, and the time elapsed is greater than our timeout
  if((doorState == DOOR_OPEN || doorState == DOOR_UNKNOWN) && (millis() - lastDoorMoveTime > doorOpenTimeout)) {
    Serial.print("ALERT -- Door is left open");
    doorState = 5;
    sendDoorState();
  }
}

void closeDoor() {
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
  for (int i=0;i<length;i++) {
    char receivedChar = (char)payload[i];
    Serial.print(receivedChar);
  }
  Serial.println();
  
  // Later parse request, it could be a command to close door
  // For now, assume all requests are queries for current state
  sendDoorState();

  // For testing only also trigger door movement every time
  // Also needs to be authenticated
  if(doorState != DOOR_CLOSED) {
    closeDoor();
  }
}

// Had error "ISR not in IRAM!" during execution after upgrading ESP Core
// Need to add ICACHE_RAM_ATTR before function as per this page:
// https://community.blynk.cc/t/error-isr-not-in-iram/37426/13
ICACHE_RAM_ATTR void closeChange() {
  closePinTriggered = true;
}

ICACHE_RAM_ATTR void openChange() {
  openPinTriggered = true;
}
