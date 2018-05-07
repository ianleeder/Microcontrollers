/*
  TheShed.h - Library for basic wifi functions.
  Created by Ian Leeder, 2018/05/05.
*/
#ifndef TheShed_h
#define TheShed_h

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>

class TheShed
{
  public:
    TheShed(const char ssid[], const char key[], const char host[] = "");
    void printMacAddress();
    void printWifiDetails();
    void test();
    void getTimeFromNtp(char* buf);
    void setupMqtt(const char* deviceName, const char* server, int port, void (*callback)(char*, uint8_t*, unsigned int), const char* subTopic = "");
    void publishToMqtt(const char* topic, const char* data);
    void mqttLoop();
  private:
    char _ssid[50];
    char _key[50];
    char _mqttSub[50];
    char _mqttDeviceName[50];
    Timezone* ausET;
    void connectWifi();
    void disconnectWifi();
    void checkWifi();
    void reconnectMqtt();
    time_t getNtpTime();
    void sendNTPpacket(IPAddress &address);
};

#endif
