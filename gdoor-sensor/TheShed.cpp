#include "Arduino.h"
#include "TheShed.h"
#include <TimeLib.h>
#include <Timezone.h>

unsigned int localPort = 2390; // local port to listen for UDP packets
IPAddress ntpServerIP; // NTP server address
const char* ntpServerName = "au.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP udp; // A UDP instance to let us send and receive packets over UDP

TheShed::TheShed(const char ssid[], const char key[], const char host[])
{
  strcpy(_key, key);
  strcpy(_ssid, ssid);
 
  if(strlen(host)>0) {
    Serial.print("Setting hostname: ");
    Serial.println(host);
    WiFi.hostname(host);
  }
  //Australia Eastern Time Zone (Sydney, Melbourne)
  TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    //UTC + 11 hours
  TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    //UTC + 10 hours
  ausET =  new Timezone(aEDT, aEST);

  udp.begin(localPort);
  //setSyncProvider(this->getNtpTime);

  connectWifi();
  
  // Resolve the NTP hostname to an IP address
  WiFi.hostByName(ntpServerName, ntpServerIP);
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

void TheShed::getTimeFromNtp(char* buf) {
  checkWifi();
  time_t utcTime = getNtpTime();
  TimeChangeRule *tcr;
  time_t localTime = ausET->toLocal(utcTime, &tcr);
  sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d %s (UTC%+d)", year(localTime), month(localTime), day(localTime), hour(localTime), minute(localTime), second(localTime), tcr->abbrev, tcr->offset/60);
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

/*-------- NTP code ----------*/

time_t TheShed::getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      unsigned long unixTime = secsSince1900 - 2208988800UL; // this is UTC
      return unixTime;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void TheShed::sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

