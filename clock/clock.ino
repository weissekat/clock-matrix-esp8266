/*
 * GPIO16 DIN
 * GPIO15 CLK
 * GPIO2  LOAD
 * GPIO5  I2C SCL
 * GPIO4  I2C SDA
 */

#include <SPI.h>
#include <bitBangedSPI.h>
#include <MAX7219_Dot_Matrix.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <MD_DS1307.h>
#include <Wire.h>

char ssid[] = "";  // AP SSID
char pass[] = "";  // AP PWD

const int MAX_WIFI_RETRY_COUNT = 5;
const int MAX_NTP_RETRY_COUNT = 5;

const char TIME_ZONE = 3;

IPAddress timeServerIP;
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[ NTP_PACKET_SIZE];

unsigned int localUdpPort = 2390;
WiFiUDP udp;

MAX7219_Dot_Matrix myDisplay (8, 2, 16, 15); // 8 chips, LOAD, DIN, CLK

void setup () {
    myDisplay.begin();
    myDisplay.setIntensity(0);
      
    WiFi.begin(ssid, pass);
    int wifi_retry_count = 0;
    while ( ( WiFi.status() != WL_CONNECTED ) && ( wifi_retry_count < MAX_WIFI_RETRY_COUNT ) ) {
        delay(1000);
        wifi_retry_count++;
    }

    if ( WiFi.status() == WL_CONNECTED ) {
        myDisplay.sendString("wifi  OK");
            delay(1000);
        
            udp.begin(localUdpPort);
          
            int received = 0;
            int ntp_retry_count = 0;
            while ( !received && ( ntp_retry_count < MAX_NTP_RETRY_COUNT ) ) {
                WiFi.hostByName(ntpServerName, timeServerIP);
                sendNTPpacket(timeServerIP);
                delay(1000);
                received = udp.parsePacket();
                if (received) break;
                delay(10000);
                ntp_retry_count++;
            }
    
            if (received) {
                udp.read(packetBuffer, NTP_PACKET_SIZE);
                unsigned long hiWord = word(packetBuffer[40], packetBuffer[41]);
                unsigned long loWord = word(packetBuffer[42], packetBuffer[43]);
                unsigned long secsSince1900 = hiWord << 16 | loWord;
                const unsigned long seventyYears = 2208988800UL;
                unsigned long epoch = secsSince1900 - seventyYears;
              
                unsigned char hour = (epoch  % 86400L) / 3600 + TIME_ZONE;
                unsigned char minute = (epoch  % 3600) / 60;
                unsigned char second = epoch % 60;
              
                if (!RTC.isRunning()) {
                    RTC.control(DS1307_CLOCK_HALT, DS1307_OFF); // turn clock ON
                    RTC.control(DS1307_SQW_RUN, DS1307_OFF);    // turn SQW output OFF
                    myDisplay.sendString("rtc   OK");
                    delay(1000);
                };
              
                RTC.h = hour;
                RTC.m = minute;
                RTC.s = second;
                RTC.writeTime();
                
                myDisplay.sendString("sync  OK");
            } else {
                myDisplay.sendString("sync BAD");
            }
        WiFi.disconnect();
    } else {
        myDisplay.sendString("wifi BAD");
    }

    delay(1000);
}

void loop () {
    char time[8];
    RTC.readTime();
    sprintf(time, "%02u    %02u", RTC.h, RTC.m);
    myDisplay.sendString(time);
  
    delay(1000);
}

unsigned long sendNTPpacket(IPAddress& address)
{
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
