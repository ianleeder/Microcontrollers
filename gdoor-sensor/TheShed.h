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
  private:
    char _ssid[50];
    char _key[50];
    Timezone* ausET;
    void connectWifi();
    void disconnectWifi();
    void checkWifi();
    time_t getNtpTime();
    void sendNTPpacket(IPAddress &address);
};

#endif
