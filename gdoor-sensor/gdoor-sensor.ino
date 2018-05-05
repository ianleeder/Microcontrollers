#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "TheShed.h"

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
const char wifiHostname[] = "gdoor-sensor";

volatile byte previousDoorState = 0;
volatile byte doorState = 0;
volatile bool publishChange = false;

WiFiClient espClient;
PubSubClient mqtt(espClient);

void setup() {
  Serial.begin(115200);
  delay(500);

  TheShed shedWifi(WIFI_SSID, WIFI_KEY, wifiHostname);
  //shedWifi.connectWifi();
  char c[40];
  shedWifi.getTimeFromNtp(c);
  Serial.println("Time is: ");
  Serial.println(c);

  // Set up MQTT
  mqtt.setServer(mqttServer, 1883);
  mqtt.setCallback(mqttCallback);

  // Set up pins
  pinMode(openPin, INPUT_PULLUP);
  pinMode(closePin, INPUT_PULLUP);
  pinMode(activatePin, OUTPUT);
  digitalWrite(activatePin, HIGH);

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
  if (!mqtt.connected()) {
    reconnectMqtt();
  }
 
  if(publishChange) {
    Serial.print("Door state changed: ");
    Serial.println(doorState);
    publishToMqtt(doorState);
    publishChange = false;
  }
}

void publishToMqtt(byte state) {
  // Prepare a JSON payload string
  Serial.print("Sending state: ");
  Serial.println(state);
  String payload = "{";
  payload += "\"state\":";
  payload += state;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  mqtt.publish( "gdoor", attributes );
  Serial.println( attributes );
}

void reconnectMqtt() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect("gdoorsensor")) {
      Serial.println("connected");
      // ... and subscribe to topic
      mqtt.subscribe("ledStatus");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
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


