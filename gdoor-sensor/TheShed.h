/*
  TheShed.h - Library for basic wifi functions.
  Created by Ian Leeder, 2018/05/05.
*/
#ifndef TheShed_h
#define TheShed_h

#include "Arduino.h"

class TheShed
{
  public:
    TheShed(const char ssid[], const char key[], const char host[] = "");
    void connectWifi();
    void disconnectWifi();
    void printMacAddress();
    void printWifiDetails();
    void getTimeFromNtp(char* buf);
    void test();
  private:
    char _ssid[50];
    char _key[50];
    inline int getSeconds(uint32_t UNIXTime);
    inline int getMinutes(uint32_t UNIXTime);
    inline int getHours(uint32_t UNIXTime);
};

#endif
