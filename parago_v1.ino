//*** Board: NodeMCU 0.9 (ESP-12 Module) ***

//----------------- libraries --------------------
// WiFi connection
#include <ESP8266WiFi.h>

// WiFi manager 
#include <WiFiManager.h>

// SW serial for HC12 RF433MHz
// increase the buffer size in C:\Users\RHRIB\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.1\libraries\SoftwareSerial\src\SoftwareSerial.h
// line 109:         int bufCapacity = 255, int isrBufCapacity = 0);
#include <SoftwareSerial.h>

// temperature sensor DS18B20
#include <DallasTemperature.h>
#include <OneWire.h>

// BME280
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// file system LittleFS 
#include <LittleFS.h>


//----------------- presets --------------------
// ESP pins running the SW serial
SoftwareSerial hc12(D6, D7);
// SET pin for the HC-12 RF module
int SETPin = D5;
// do not use, LED on the blue board
#define LED 2
// variable for the HC12 parser
String message = "";
// delimiter for the HC12 parser
char MyD[] = "&";

// LittleFS setting file path
String lfsSettingsFile = "/esppresets.ini";

// external logging system URL 
String http2_site; 
//char http2_site[] = "aaa.bbb.cc"; //-> now read from LittleFS file

// altitude at your place (replace with your values as needed)
const float vyska = 200;                      

// web server (port 80)
WiFiServer server(80);
String readStrings = ""; // GET response  (ie. button from ESP's webserver)

// input pin for DS18B20
const int pinCidlaDS = D2;

// oneWireDS instance (OneWire library)
OneWire oneWireDS(pinCidlaDS);

// sensorsDS instance (DallasTemperature library) and variables
DallasTemperature sensorsDS(&oneWireDS);
float teplotaDS;   // DS18B20 temprature (float) 
String termOUTWWW; // DS18B20 temprature (String)

// BME280 sensor address on the I2C bus, Adafruit_BME280 instance
#define BME280_ADRESA (0x76)
Adafruit_BME280 bme;

// WiFiManager instance
WiFiManager wifiManager;


//----------------- variables --------------------
char Signal[6];  // WiFi signal strength
String id1, T1, T2, H, P, ilum, DP, alarm1, alarm2, alarm3, alarm4;
float h2, tem2, p2, pin2, dp2, ilum2;
int vstupy;
char temperatureFString[6];
char dpString[6];
char humidityString[6];
char pressureString[7];
char ilumstring[6];
char vystupstring[6];
int inPin2 = D6; // input PIN

unsigned long aktualniMillis;  // current time
unsigned long predchoziMillisLong; // last event - long loop
unsigned long predchoziMillisShort; // last event - short loop
float longLoop = 1 * 60 * 1000;  // 1 * 1 min
float shortLoop = 0.1 * 60 * 1000;  // 0.1 * 1 min

int kotelout = -1; 
int termostatout = -1;
int kotelin = -1; 
int termostatin = -1;
int sendout = 0;


//----------------- setup() --------------------
// code to run once (after boot)
void setup() {
    Serial.begin(9600);  // serial speed for USB serial terminal

    //-- load presets from file system  
    if (!LittleFS.begin()) {
      Serial.println("An error has occurred while mounting LittleFS");
      return;
    }      

    File file = LittleFS.open(lfsSettingsFile, "r");    
    if (!file) {
        Serial.println("Failed to open file");
        return;
    }
    
    http2_site = file.readStringUntil('\n');

    file.close(); // close the file

    //-- turn on and connect to WiFi 
    wifiManager.autoConnect("AutoConnectAP");

    //-- start the web server
    server.begin();
    Serial.println("Web server running. Waiting for IP...");
    delay(1000);

    //-- init HC12
    pinMode(SETPin, OUTPUT);
    // pinMode(LED, OUTPUT);
    digitalWrite(SETPin, HIGH);
    hc12.begin(9600);
    Serial.println("Configuration module HC-12");
    digitalWrite(SETPin, LOW);
    delay(100);

    hc12.print("AT+C001");
    delay(100);
    if (hc12.available()) {
        message = hc12.readString();
        Serial.println(message);
    }
    
    hc12.print("AT+P4");
    delay(100);
    if (hc12.available()) {
        message = hc12.readString();
        Serial.println(message);
    }

    hc12.print("AT+FU3");
    delay(100);
    if (hc12.available()) {
        message = hc12.readString();
        Serial.println(message);
    }

    digitalWrite(SETPin, HIGH);
    delay(1000);

    //-- initiate the thermal sensor
    sensorsDS.begin();
    
    //-- initiate I2C bus
    Wire.begin(D3, D4); // sda, scl
    Wire.setClock(100000);
    
    //-- initiate BME280
    if (!bme.begin(BME280_ADRESA)) {
        Serial.println("BME280 sensor not detected (check connectivity)");
        return;
    }
}


//----------------- Other Routines --------------------

//**** connect to WiFi (25 tries) [-NOT USED-]
// void WIFI_Connect() {
//     if (WiFi.status() != WL_CONNECTED) {
//         WiFi.disconnect();
//         Serial.println("Reconnecting WiFi");
//         WiFi.mode(WIFI_STA);
//         WiFi.begin(ssid, password);
//         // Wait for connection
//         for (int i = 0; i < 25 && WiFi.status() != WL_CONNECTED; i++) {
//             delay(500);
//             Serial.print(".");
//         }
//     }
//     if (WiFi.status() != WL_CONNECTED) { return; }
// }


//***** html page (ESP webserver content)
void xicht() {

    // responds to http GET request to the esp port 80 
    WiFiClient client = server.accept();

    if (client) {
        Serial.println("New client");
        // bolean to locate when the http request ends
        boolean blank_line = true;
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();

                if (readStrings.length() < 100) {
                    readStrings += c;
                }

                if (c == '\n' && blank_line) {

                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");
                    client.println();
                    // your actual web page that displays temperature
                    client.println("<!DOCTYPE HTML>");
                    client.println("<html>");
                    client.println("<head><META HTTP-EQUIV=\"refresh\" CONTENT=\"15\"><title>Arduino ESP-B67681</title></head>");
                    client.println("<body><h1>WiFi Gateway</h1>");
                    client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
                    client.println("<h3>RF433MHz ");
                    client.println("<h3>Kotel: ");
                    client.println(String(kotelout));
                    client.println(" </h3><h3>Termostat: ");
                    client.println(String(termostatout));
                    client.println(" </h3><h3>");
                    client.println("</h3></td></tr></tbody></table></body></html>");
                    client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
                    client.println("<h3>ID RF433MHz: ");
                    client.println(" ");
                    client.println(" ");
                    client.println("</h3></td></tr></tbody></table></body></html>");

                    client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
                    client.println("<h3>Local meteo station");
                    client.println("<h3>Teplota : ");
                    client.println(termOUTWWW);
                    client.println("&deg;C </h3><h3>Vlhkost : ");
                    client.println(humidityString);
                    client.println("%</h3><h3>Tlak : ");
                    client.println(pressureString);
                    client.println("hPa</h3><h3>Rosny Bod : ");
                    client.println(dpString);
                    client.println("&deg;C </h3><h3>Location : ");
                    client.println(vyska);
                    client.println("m.n.m</h3></td></tr></tbody></table></body></html>");
                    client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
                    client.println("<h3>Napeti A0 : ");
                    client.println(ilumstring);
                    client.println(" V       ");
                    client.println("</h3><h3>Digital PIN D6 : ");
                    client.println(vystupstring);
                    client.println("</h3></td></tr></tbody></table></body></html>");

                    client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
                    client.println("<h3>Network : ");
                    client.println(wifiManager.getSSID());
                    client.println("  ");
                    // client.println(WiFi.localIP());
                    client.println("  Signal : ");
                    client.println(String(Signal));
                    client.println("dBm");
                    client.println("<h3>Exporting data : ");
                    client.println(http2_site);
                    client.println("</h3></td></tr></tbody></table></body></html>");
                    client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
                    client.print("<p>LEDka pro Mamu? <a href=\"/?button2on\"><button>ON</button></a>&nbsp;<a href=\"/?button2off\"><button>OFF</button></a></p>");
                    // client.print("<br />\r\n");
                    //  client.println(rele2string);
                    client.println("</h3></td></tr></tbody></table></body></html>");
                    client.print("</BODY>\r\n");
                    client.print("</HTML>\n");

                    break;
                }
                if (c == '\n') {
                    // when starts reading a new line
                    blank_line = true;
                } else if (c != '\r') {
                    // when finds a character on the current line
                    blank_line = false;
                }
            }
        }
        // closing the client connection
        delay(1);
        client.stop();
        Serial.println("Client disconnected.");

        // code to run when ?button2on
        if (readStrings.indexOf("?button2on") > 0) {
        }
        // code to run when ?button2off
        if (readStrings.indexOf("?button2off") > 0) {
        }

        readStrings = "";
    }
}

//***** log boiler data to external logging service
void data_out() {
    // make the HTTP GET request to http2_site:80
    WiFiClient client2;
    if (!client2.connect(http2_site, 80)) {
        Serial.println("Site connection error");
    }
    String request2 = "/insert_rada2.php?A1=" + String(kotelout) + "&A2=" + String(termostatout) + "&A3=" + String(kotelin) + "&A4=" + String(termostatin);

    Serial.println(request2);
    client2.print(String("GET ") + request2 + " HTTP/1.1\r\n" +
                  "Host: " + http2_site + "\r\n" +
                  "Connection: close\r\n\r\n");

    delay(500);
    while (client2.available()) {
        String line = client2.readStringUntil('\r');
        Serial.print(line);
    }

    if (client2.connected()) {
        client2.stop(); // DISCONNECT FROM THE SERVER
    }
}


//***** log meteo data to external logging service
void data_out_meteo() {
    // make the HTTP GET request http2_site:80
    WiFiClient client3;
    if (!client3.connect(http2_site, 80)) {
        Serial.println("Site connection error");
    }
    String request2 = "/insert_rada1.php?temp=" + String(termOUTWWW) + "&humi=" + String(h2) + "&pres=" + String(p2) + "&devp=" + String(dp2) + "&ilum=" + String(ilum2);

    Serial.println(request2);
    client3.print(String("GET ") + request2 + " HTTP/1.1\r\n" +
                  "Host: " + http2_site + "\r\n" +
                  "Connection: close\r\n\r\n");

    delay(500);
    while (client3.available()) {
        String line = client3.readStringUntil('\r');
        Serial.print(line);
    }

    if (client3.connected()) {
        client3.stop(); // DISCONNECT FROM THE SERVER
    }
}

//***** read and parse data from the HC12 [-NOT USED-]
void parser() {

    int message_len = message.length() + 1;
    char char_array[message_len];
    message.toCharArray(char_array, message_len);

    char *s = strtok(char_array, MyD);
    Serial.println(s);
    id1 = String(s);

    char *s1 = strtok(NULL, MyD);
    Serial.println(s1);
    T1 = String(s1);

    char *s2 = strtok(NULL, MyD);
    Serial.println(s2);
    T2 = String(s2);

    char *s3 = strtok(NULL, MyD);
    Serial.println(s3);
    H = String(s3);

    char *s4 = strtok(NULL, MyD);
    Serial.println(s4);
    P = String(s4);

    char *s5 = strtok(NULL, MyD);
    Serial.println(s5);
    DP = String(s5);

    char *s6 = strtok(NULL, MyD);
    Serial.println(s6);
    ilum = String(s6);

    char *s7 = strtok(NULL, MyD);
    Serial.println(s7);
    alarm1 = String(s7);

    char *s8 = strtok(NULL, MyD);
    Serial.println(s8);
    alarm2 = String(s8);

    char *s9 = strtok(NULL, MyD);
    Serial.println(s9);
    alarm3 = String(s9);

    char *s10 = strtok(NULL, MyD);
    Serial.println(s10);
    alarm4 = String(s10);
}


//***** read the temperature (DS18B20)
void teplotaDS18B20() {
    // read the info available from all sensors connected to given pin
    sensorsDS.requestTemperatures();
    // read the value from sensor(0)
    teplotaDS = sensorsDS.getTempCByIndex(0);
    Serial.print("Teplota cidla DS18B20: ");
    Serial.print(teplotaDS);
    Serial.println(" °C");

    termOUTWWW = String(teplotaDS, 1);
    // teplota2 = senzoryDS.getTempCByIndex(1);
    // termINWWW = String(teplota2, 1);

    delay(1000);
}


//***** read the humidity, temperature (bme), air pressure and light level
void getWeather() {

    h2 = bme.readHumidity();
    tem2 = bme.readTemperature();

    dp2 = teplotaDS - ((100 - h2) / 5); // t-0.36*(100.0-h);

    p2 = (bme.readPressure() / 100.0F) + (vyska / 8.3);
    // pin2 = 0.02953*p2;

    ilum2 = analogRead(A0); // read the input pin

    dtostrf(tem2, 5, 1, temperatureFString);
    dtostrf(h2, 5, 1, humidityString);
    dtostrf(p2, 6, 1, pressureString);
    dtostrf(dp2, 5, 1, dpString);
    dtostrf(ilum2, 5, 1, ilumstring);
    vstupy = 0;

    vstupy = digitalRead(inPin2);

    dtostrf(vstupy, 5, 1, vystupstring);
    // delay(100);

    Serial.println("BME280: ");
    Serial.print("T=");
    Serial.print(temperatureFString);
    Serial.println(" °C");
    Serial.print("H=");
    Serial.print(humidityString);
    Serial.println(" %");
    Serial.print("P=");
    Serial.print(pressureString);
    Serial.println(" hPa");
    Serial.print("T=");
    Serial.print(dpString);
    Serial.println(" °C");
    Serial.print("Fotoodpor: ");
    Serial.print(ilum2);
    Serial.println(" ");
}


//----------------- loop() --------------------
//***** main code to be run repeatedly in infiinite loop
void loop() {
    xicht();

    aktualniMillis = millis(); // podivam se na hodinky

    if (aktualniMillis - predchoziMillisShort  > shortLoop) { // repeat 10x per min
        predchoziMillisShort = aktualniMillis;                // timestamp of last execution
 
        if (hc12.available()) {
            message = "";
            String chunk;
            do {
                chunk = hc12.readString(); 
                message = message + chunk;
            } while (chunk.length() > 0 && hc12.available());
            //message = hc12.readString(); 

            Serial.println(message);

            if (message.indexOf("KOTEL001=0") > -1) { kotelout = 0; sendout = 1;}
            if (message.indexOf("KOTEL001=1") > -1) { kotelout = 1;  sendout = 1;}

            if (message.indexOf("TERMO001=0") > -1) { termostatout = 0;  sendout = 1;}
            if (message.indexOf("TERMO001=1") > -1) { termostatout = 1;  sendout = 1;}

            if (message.indexOf("OK0") > -1) { kotelin = 0;  sendout = 1;}
            if (message.indexOf("OK1") > -1) { kotelin = 1;  sendout = 1;}

            if (message.indexOf("/OK/0/") > -1) { termostatin = 0;  sendout = 1;}
            if (message.indexOf("/OK/1/") > -1) { termostatin = 1;  sendout = 1;}

        }
    }

    if (aktualniMillis - predchoziMillisLong  > longLoop) { // repeat 1x per min
        predchoziMillisLong = aktualniMillis;               // timestamp of last execution
        
        Serial.println("1st line of 'esppresets.ini': " + http2_site);

        if (sendout == 1) {
            sendout = 0;
            data_out();
            delay(500);
        }

        // hc12.print("X1G001S1");
        int8_t rssi = wifiManager.getRSSI();
        dtostrf(rssi, 5, 1, Signal);

        teplotaDS18B20();
        delay(1000);
        getWeather();
        delay(1000);
        data_out_meteo();
    }
    
}
