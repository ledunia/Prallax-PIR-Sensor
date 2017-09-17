/*
   ++++++++++++++++++++++++++++++++++++++++++++++++++++
   Making sense of the PRTG HTTP Push Data Beta Sensor
   with Parallax PIR sensor's output as data source.
   ++++++++++++++++++++++++++++++++++++++++++++++++++++

   Sending http push data to PRTG, according to the state of the sensors output pin.
   Determines the beginning and end of continuous motion sequences.

   @author: Christian Zeh
   @date:   17. September 2017

   released under a creative commons "Attribution-NonCommercial-ShareAlike 2.0" license
   http://creativecommons.org/licenses/by-nc-sa/2.0/de/


   The Parallax PIR Sensor is an easy to use digital infrared motion sensor module.
   (http://www.parallax.com/detail.asp?product_id=555-28027)

   The sensor's output pin goes to HIGH if motion is present.
   However, even if motion is present it goes to LOW from time to time,
   which might give the impression no motion is present.
   This program deals with this issue by ignoring LOW-phases shorter than a given time,
   assuming continuous motion is present during these phases.

*/

// Include Libraries
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <WiFiManager.h>      //Version 0.10.0
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include "FastLED.h"          //Version 3.1.4

String IP; //IP address converted from type IPAddress to String to replace in HTML
const char hosttarget[] = "192.168.188.34";
String token = "pir-token-push";
ESP8266WebServer server(80);
MDNSResponder mdns;
#define NUM_LEDS 4
#define DATA_PIN 4
CRGB leds[NUM_LEDS];

//the time we give the sensor to calibrate (10-60 secs according to the datasheet)
int calibrationTime = 30;

//the time when the sensor outputs a low impulse
long unsigned int lowIn;

//the amount of milliseconds the sensor has to be low
//before we assume all motion has stopped
long unsigned int pause = 5000;

boolean lockLow = true;
boolean takeLowTime;

int pirPin = 12;    //the digital pin connected to the PIR sensor's output
int ledPin = 13;    //the onboard led pin

//Setup:
void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.begin(9600);

  //WiFiManager:
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  //---->reset saved settings
  //wifi.resetSettings();
  //set custom ip for portal
  //wifi.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "pir-sensor.local"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("pir-sensor");
  //or use this for auto generated name ESP + ChipID
  //wifi.autoConnect();
  //if you get here you have connected to the WiFi
  Serial.println("pir-sensor connected...");


  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.print(".");
  }

  if ( !mdns.begin ( "pirsensor", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder started" );
  }

  // Start the server
  server.begin();
  Serial.println("Webserver ready");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  IPAddress localIP = WiFi.localIP();
  IP = String(localIP[0]);
  IP += ".";
  IP += String(localIP[1]);
  IP += ".";
  IP += String(localIP[2]);
  IP += ".";
  IP += String(localIP[3]);

  pinMode(pirPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(pirPin, LOW);

  //give the sensor some time to calibrate
  Serial.print("calibrating sensor ");
  for (int i = 0; i < calibrationTime; i++) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" done");
  Serial.println("SENSOR ACTIVE");
  delay(50);
}

////////////////////////////
//LOOP
void loop() {

  // read the input on analog pin 0:
  int sensorValue = analogRead(A0);
  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  //float voltage = sensorValue * (5.0 / 1023.0);
  float voltage = sensorValue * (10 / 512.5);
  int ldr = (int)voltage;

  // Use WiFiClient class to create TCP connections

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 5050; // http push data sensor port for PRTG
  if (!client.connect(hosttarget, httpPort)) {
    Serial.println("connection failed");
    return;
  }


  for (int i = 0; i < NUM_LEDS; i++) {
    if (digitalRead(pirPin) == HIGH) {
      leds[i] = CRGB::Red;
      FastLED.show();
    } else if (digitalRead(pirPin) == LOW) {
      leds[i] = CRGB::Green;
      FastLED.show();
    }
  }

  if (digitalRead(pirPin) == HIGH) {

    //token + value + text
    String data = "?content=<prtg><result><channel>Motion</channel><value>";
    String restdata = "</value></result><text>Motion</text><result><channel>LDR</channel><value>";
    String data1 = "<result><channel>ldr</channel><value>";
    String restdata1 = "</value></result><text>ldr%20aka%20LUX</text></prtg>";

    // We now create a URI for the request
    String url = "GET /" + token + data + 1 + restdata + ldr + restdata1 + " HTTP/1.1";
    client.println(url);
    client.println("Host: transmitter.local");
    client.println("Connection: close");
    client.println();
    delay(500);
    digitalWrite(ledPin, HIGH);   //the led visualizes the sensors output pin state
    if (lockLow) {
      //makes sure we wait for a transition to LOW before any further output is made:
      lockLow = false;
      Serial.println("---");
      //Serial.print (ldr);
      Serial.print("motion detected at ");
      Serial.print(millis() / 1000);
      Serial.println(" sec");
      delay(50);
    }
    takeLowTime = true;
  }

  if (digitalRead(pirPin) == LOW) {

    // We now create a URI for the request
    String data = "?content=<prtg><result><channel>Motion</channel><value>";
    String restdata = "</value></result><text>Motion</text><result><channel>LDR</channel><value>";
    String data1 = "<result><channel>ldr</channel><value>";
    String restdata1 = "</value></result><text>ldr%20aka%20LUX</text></prtg>";

    // We now create a URI for the request
    //  String url = "GET /" + data + " HTTP/1.1";
    String url = "GET /" + token + data + 0 + restdata + ldr + restdata1 + " HTTP/1.1";

    client.println(url);
    client.println("Host: pirsensor.local");
    client.println("Connection: close");
    client.println();
    delay(500);
    digitalWrite(ledPin, LOW);  //the led visualizes the sensors output pin state

    if (takeLowTime) {
      lowIn = millis();          //save the time of the transition from high to LOW
      takeLowTime = false;       //make sure this is only done at the start of a LOW phase
    }
    //if the sensor is low for more than the given pause,
    //we assume that no more motion is going to happen
    if (!lockLow && millis() - lowIn > pause) {
      //makes sure this block of code is only executed again after
      //a new motion sequence has been detected
      lockLow = true;
      Serial.print("motion ended at ");      //output
      Serial.print((millis() - pause) / 1000); //1 min
      Serial.println(" sec");
      delay(50);
    }
  }
}
