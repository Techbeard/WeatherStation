#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "LedControl.h"
#include "DHT.h"
#include <config.h>
#include <ArduinoJson.h>

ADC_MODE(ADC_VCC);

#define DHTPIN D5
#define DHTTYPE DHT22 //DHT11, DHT21, DHT22

DHT dht(DHTPIN, DHTTYPE);

const unsigned long tempUpdateInterval = 120000;


LedControl lc=LedControl(D7,D8,D6,2);


// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";


WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void ClockDisplay();
void sendNTPpacket(IPAddress &address);

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

WiFiClient client;

#define VERBOSE 1

float humid = 0, temp = 0, outsideTemp = 0;


void setup()
{
        Serial.begin(9600);
        while (!Serial) ; // Needed for Leonardo only
        delay(250);
        Serial.println("TimeNTP Example");
        Serial.print("Connecting to ");
        Serial.println(ssid);
        WiFi.begin(ssid, pass);

        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
        }

        Serial.print("IP number assigned by DHCP is ");
        Serial.println(WiFi.localIP());
        Serial.println("Starting UDP");
        Udp.begin(localPort);
        Serial.print("Local port: ");
        Serial.println(Udp.localPort());
        Serial.println("waiting for sync");
        setSyncProvider(getNtpTime);
        setSyncInterval(300);

        lc.shutdown(0,false);
        lc.shutdown(1,false);
        lc.setIntensity(0,8);
        lc.setIntensity(1,8);
        lc.clearDisplay(0);
        lc.clearDisplay(1);

        getTemp();
}

time_t prevDisplay = 0; // when the digital clock was displayed
unsigned long lastTempMeasure = 0;

void loop()
{
        if (timeStatus() != timeNotSet) {
                if (now() != prevDisplay) { //update the display only if time has changed
                        prevDisplay = now();
                        ClockDisplay();
                }
        }

        if(millis() - lastTempMeasure >= tempUpdateInterval) {
                lastTempMeasure = millis();
                getTemp();
        }

}

void getTemp()
{
        byte timeout = 0;
        do {
                humid = dht.readHumidity(); //Luftfeuchte auslesen
                temp  = dht.readTemperature(); //Temperatur auslese
                if(VERBOSE)
                        Serial.println(temp);
                delay(100);
                timeout++;
                if(timeout==20) {
                        break;
                }
        } while(isnan(temp));
        if(!isnan(temp))
                sendData(temp, humid); //send data to influxDB

        outsideTemp = getOutTemp();
}

//---------------_Ausgabe und berechnung der einzel zahlen ------------------

void ClockDisplay()
{
        //Zeit
        byte h10 = int(hour() / 10); // Ziffer der Zehnerstelle berechnen
        byte h1 = hour() - h10 * 10;
        byte m10 = int(minute() / 10); // Ziffer der Zehnerstelle berechnen
        byte m1 = minute() - m10 * 10;
        byte s10 = int(second() / 10); // Ziffer der Zehnerstelle berechnen
        byte s1 = second() - s10 * 10;

        //Date
        byte day10 = int(day() / 10); // Ziffer der Zehnerstelle berechnen
        byte day1 = day() - day10 * 10;
        byte month10 = int(month() / 10); // Ziffer der Zehnerstelle berechnen
        byte month1 = month() - month10 * 10;


        //---------------zahlenausgabe---------------------------

        
        lc.setDigit(1,4,m1,false);
        lc.setDigit(1,5,m10,false);
        lc.setDigit(1,6,h1,true);
        lc.setDigit(1,7,h10,false);
        lc.setDigit(0,4,month1,false);
        lc.setDigit(0,5,month10,false);
        lc.setDigit(0,6,day1,true);
        lc.setDigit(0,7,day10,false);

        printTemp(0, 0, temp);
        printTemp(1, 0, outsideTemp);

}

void printTemp(byte address, byte offset, float value) {
        if(!isnan(value)) {
                byte digits[3] = {
                        (byte)(value * 10) % 10,
                        (byte) value       % 10,
                        (byte)(value / 10) % 10,
                };

                if(value >= 0) {
                        lc.setRow(address, offset + 0, B01100011); //display degrees symbol
                        offset++; //move value one to the left
                }
                else {
                        lc.setChar(address, offset + 3, '-', false);
                }
                lc.setDigit(address, offset + 0, digits[0], false);
                lc.setDigit(address, offset + 1, digits[1], true);
                if(digits[2] != 0)
                        lc.setDigit(address, offset + 2, digits[2], false);
                else
                        lc.setChar(address, offset + 2, ' ', false);
        }
        else {
                for(byte i = 0; i < 4; i++) {
                        lc.setChar(address, offset + i, ' ', false);
                }
        }

}

float getOutTemp() {
        Serial.print("connecting to ");
        Serial.println(host);
        if (!client.connect(host, influxPort)) {
          Serial.println("connection failed");
          return NAN;
        }

        String sendStr = "GET " + String(outTempQueryUrl) + " HTTP/1.1\r\n";
        sendStr += "Host: " + String(host) + "\r\n";
        if(auth.length() > 0)
                sendStr += "Authorization: Basic " + String(auth) + "\r\n";
        //sendStr += "Connection: close\r\n";
        //sendStr += "Accept: */*\r\n";
        sendStr += "User-Agent: Admindu esp8266 sensor\r\n";
        sendStr += "Connection: close\r\n\r\n";

        if(VERBOSE) {
                Serial.print(sendStr);
        }

        client.print(sendStr);

        String json;
        while (client.connected()) {
                String line = client.readStringUntil('\n');
                //Serial.println(line);
                if(line.indexOf("{") != -1) {
                        json = line;
                        break;
                }
        }

        Serial.println(json);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& parsed = jsonBuffer.parseObject(json);
        if (!parsed.success()) {
                 Serial.println("JSON parsing failed");
                 return NAN;
        }
        //return string is: {"results":[{"series":[{"name":"Wohnung","columns":["time","last"],"values":[["2017-09-23T18:21:12.880325091Z",25.4]]}]}]}
        float outTemp = parsed["results"][0]["series"][0]["values"][0][1];
        //Serial.println(outTemp);
        return outTemp;

}

void sendData(float temp, float humid) {
        Serial.print("connecting to ");
        Serial.println(host);
        if (!client.connect(host, influxPort)) {
          Serial.println("connection failed");
          return;
        }
        // verbose
        if(VERBOSE) {
          Serial.print("requesting URL: ");
          Serial.println(url);
          Serial.println(" ");
        }

        // reading the rssi sometimes failed. the root-cause is
        // still unclear. to prevent rotten data in influx we
        // read rssi until it is not 31 which is the error code
        // the SDK returns if reading fails. Also enforcing a
        // timeout to prevent battery drain.
        long rssi = 31;
        byte timeout = 0;
        while(rssi == 31) {
                delay(50);
                rssi = WiFi.RSSI();
                timeout++;
                if(timeout==10) {
                        break;
                }
        }
        // read and calculate the battery voltage
        int heap            = ESP.getFreeHeap();
        int voltage         = ESP.getVcc();
        voltage             = voltage * 0.93;

        String data = String(measurement)
                + ",sensorName=" + String(sensorName)
                + " rssi=" + String(rssi)
                + ",voltage=" + String(voltage)
                + ",heap=" + String(heap)
                + ",temperature=" + String(temp)
                + ",humidity=" + String(humid) +"\n";

        // get the lenght of the post data an construct a http
        // post request containing authentication and the data
        int data_length = data.length();

        String sendStr = "POST " + String(url) + " HTTP/1.1\r\n";
        sendStr += "Host: " + String(host) + "\r\n";
        if(auth.length() > 0)
                sendStr += "Authorization: Basic " + String(auth) + "\r\n";
        sendStr += "Connection: close\r\n";
        sendStr += "Content-Type: application/x-www-form-urlencoded\r\n";
        sendStr += "Accept: */*\r\n";
        sendStr += "User-Agent: Admindu esp8266 sensor\r\n";
        sendStr += "Connection: close\r\n";
        sendStr += "Content-Length:" + String(data_length) + "\r\n\r\n" + String(data) + "\r\n";

        // verbose
        if(VERBOSE) {
          Serial.print(sendStr);
        }

        // send the request to the server
        client.print(sendStr);

        // verbose
        Serial.println("request sent");
        Serial.println(" ");

        // get the result from the server an parse it for code
        // 204 No Content as influxdb will answer that way if
        // everything is ok.
        while (client.connected()) {
          String line = client.readStringUntil('\n');
          Serial.println(line);
          if(line.indexOf("204 No Content") != -1) {
            Serial.println("influxdb successfull!");
          }
          if (line == "\r") {
            Serial.println("headers received");
            Serial.println(" ");
            break;
          }
        }
}


time_t getNtpTime()
{
        IPAddress ntpServerIP; // NTP server's ip address


        while (Udp.parsePacket() > 0) ; // discard any previously received packets
        Serial.println("Transmit NTP Request");
        // get a random server from the pool
        WiFi.hostByName(ntpServerName, ntpServerIP);
        Serial.print(ntpServerName);
        Serial.print(": ");
        Serial.println(ntpServerIP);
        sendNTPpacket(ntpServerIP);
        uint32_t beginWait = millis();
        while (millis() - beginWait < 1500)
        {
                int size = Udp.parsePacket();
                if (size >= NTP_PACKET_SIZE)
                {
                        Serial.println("Receive NTP Response");
                        Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
                        unsigned long secsSince1900;
                        // convert four bytes starting at location 40 to a long integer
                        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
                        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
                        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
                        secsSince1900 |= (unsigned long)packetBuffer[43];
                        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
                }
        }
        Serial.println("No NTP Response :-(");
        return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address

void sendNTPpacket(IPAddress & address)
{
        // set all bytes in the buffer to 0
        memset(packetBuffer, 0, NTP_PACKET_SIZE);
        // Initialize values needed to form NTP request
        // (see URL above for details on the packets)
        packetBuffer[0] = 0b11100011; // LI, Version, Mode
        packetBuffer[1] = 0; // Stratum, or type of clock
        packetBuffer[2] = 6; // Polling Interval
        packetBuffer[3] = 0xEC; // Peer Clock Precision
        // 8 bytes of zero for Root Delay & Root Dispersion
        packetBuffer[12] = 49;
        packetBuffer[13] = 0x4E;
        packetBuffer[14] = 49;
        packetBuffer[15] = 52;
        // all NTP fields have been given values, now
        // you can send a packet requesting a timestamp:
        Udp.beginPacket(address, 123); //NTP requests are to port 123
        Udp.write(packetBuffer, NTP_PACKET_SIZE);
        Udp.endPacket();
}
