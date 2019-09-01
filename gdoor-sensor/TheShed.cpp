#include "Arduino.h"
#include "TheShed.h"
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient mqtt(espClient);

TheShed::TheShed(const char ssid[], const char key[], const char host[])
{
  strcpy(_key, key);
  strcpy(_ssid, ssid);
 
  if(strlen(host)>0) {
    Serial.print("Setting hostname: ");
    Serial.println(host);
    WiFi.hostname(host);
  }

  connectWifi();
}

void TheShed::setupMqtt(const char* deviceName, const char* server, int port, void (*callback)(char*, uint8_t*, unsigned int), const char subTopic[]) {
  // Set up MQTT
  mqtt.setServer(server, port);
  mqtt.setCallback(callback);

  strcpy(_mqttSub, subTopic);
  strcpy(_mqttDeviceName, deviceName);
  reconnectMqtt();
}

void TheShed::mqttLoop() {
  reconnectMqtt();
  mqtt.loop();
}

void TheShed::reconnectMqtt() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(_mqttDeviceName)) {
      Serial.println("connected");
      if(strlen(_mqttSub)){
        mqtt.subscribe(_mqttSub);
        Serial.print("Subscribed to topic: ");
        Serial.println(_mqttSub);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void TheShed::publishToMqtt(const char* topic, const char* data) {
  reconnectMqtt();
  // Prepare a JSON payload string
  Serial.print("Publishing to topic: ");
  Serial.println(topic);
  Serial.print("Sending data: ");
  Serial.println(data);
  mqtt.publish( topic, data );
}

void TheShed::test() {
  
}

void TheShed::checkWifi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi not connected, reinitialising");
    connectWifi();
  } else {
    Serial.println("Wifi still connected");
  }
}

void TheShed::connectWifi() {
  printMacAddress();
  
  // Connect wifi
  Serial.println("Initialising wifi");
  WiFi.mode(WIFI_STA);
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
