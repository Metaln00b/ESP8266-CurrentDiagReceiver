#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncElegantOTA.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiGratuitous.h>
#include <TFT_eSPI.h>       // Hardware-specific library
#include <SPI.h>

#define DRAW_DIGITS

#ifdef DRAW_DIGITS
  #include "NotoSans_Bold.h"
  #include "OpenFontRender.h"
  #define TTF_FONT NotoSans_Bold
#endif

TFT_eSPI display = TFT_eSPI();  // Invoke custom library

TFT_eSprite spr = TFT_eSprite(&display);  // Declare Sprite object "spr" with pointer to "tft" object

#ifdef DRAW_DIGITS
OpenFontRender ofr;
#endif

const char* ssid = "CurrentDiagReceiver";
const char* pass = "123456789";

AsyncWebServer server(80);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 280
#define SCREEN_OFFSET 50
//#define TFT_BL D2

#define SPEED_SENSOR_PIN D2
#define K_FACTOR 4127.0

volatile unsigned int pulseCount = 0;
unsigned long lastPulseTime = 0;
float speed_kmh = 0;

#define AUDI_RED 0x4800 //RGB565
#define AUDI_HIGHLIGHTED_RED 0xF980 //RGB565

const int EXDURATION = 2;
const int TIMEOUT = 2;
const int PORT = 8266;

float v1Min = -80;
float v1Max = 120;
float v1Vline = 0;

float v2Min = 0.6;
float v2Max = 1.4;
float v2Vline = 1.0;

WiFiUDP Udp;

void showClients() 
{
  unsigned char number_client;
  struct station_info *stat_info;
  
  struct ip4_addr *IPaddress;
  IPAddress address;
  int cnt=1;
  
  number_client = wifi_softap_get_station_num();
  stat_info = wifi_softap_get_station_info();
  
  Serial.print("Connected clients: ");
  Serial.println(number_client);

  while (stat_info != NULL)
  {
      IPaddress = &stat_info->ip;
      address = IPaddress->addr;

      Serial.print(cnt);
      Serial.print(": IP: ");
      Serial.print((address));
      Serial.print(" MAC: ");

      uint8_t *p = stat_info->bssid;
      Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", p[0], p[1], p[2], p[3], p[4], p[5]);

      stat_info = STAILQ_NEXT(stat_info, next);
      cnt++;
      Serial.println();
  }
}

void eventCb(System_Event_t *evt)
{
    switch (evt->event)
    {
      case WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP:
      case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      showClients();
      break;
      
    default:
        break;
    }
}

void initWiFi()
{
  Serial.println("Setting soft-AP ... ");
  wifi_set_event_handler_cb(eventCb);

  IPAddress apIP(192, 168, 4, 1);

  if (WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)))
    Serial.println("softAPConfig: True");
  else
    Serial.println("softAPConfig: False");

  if (WiFi.softAP(ssid, pass, 8))
    Serial.println("softAP: True");
  else
    Serial.println("softAP: False");
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  if (WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();
    WiFi.begin("CurrentDiag", pass);
    Serial.println();
    Serial.print("Wait for WiFi");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
  }

  AsyncElegantOTA.begin(&server);

  server.begin();

  Udp.begin(PORT);
  experimental::ESP8266WiFiGratuitous::stationKeepAliveSetIntervalMs(5000);
}

void printDisplay(const char *string, int duration_s) {
  display.fillScreen(AUDI_RED);
  display.setTextSize(3);
  display.setTextColor(AUDI_HIGHLIGHTED_RED);
  display.setCursor(0, 0 + SCREEN_OFFSET);
  display.println(string);
  delay(duration_s * 1000);
}

void drawValueBar(int x, int y, int width, int height, float valueMin, float valueMax, float value, float vlineValue, bool showNumbers) {
  float roundValue = min(max(valueMin, value), valueMax);
  float valueRange = valueMax - valueMin;
  float pxPerValue = (width - 4) / valueRange;
  int pxPositionVline = int(pxPerValue * (vlineValue - valueMin) + 2);
  int pxPositionValue = int(pxPerValue * (roundValue - valueMin) + 2);
  int start = min(pxPositionValue, pxPositionVline);
  int end = max(pxPositionValue, pxPositionVline);

  display.drawRect(x, y, width, height, AUDI_HIGHLIGHTED_RED);
  display.fillRect(x + start, y + 2, end - start, height - 4, AUDI_HIGHLIGHTED_RED);

  if (height >= 15 && showNumbers) {
    if (value >= vlineValue) {
      display.setTextSize(3);
      display.setTextColor(AUDI_HIGHLIGHTED_RED);
      display.setCursor(int(width / 9), y + 5);
      display.println(String(roundValue, 2));
    } else {
      display.setTextSize(3);
      display.setTextColor(AUDI_HIGHLIGHTED_RED);
      display.setCursor(int(width / 1.5), y + 5);
      display.println(String(roundValue, 2));
    }
  }

  display.drawFastVLine(pxPositionVline, y - 1, height + 2, AUDI_HIGHLIGHTED_RED);
}

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
  {
    ; // Needed for native USB port only
  }

  //analogWrite(TFT_BL, 255);
  
  display.init();
  display.setRotation(1);
  display.setTextColor(AUDI_HIGHLIGHTED_RED);
  display.fillScreen(AUDI_RED);
  display.setTextSize(3);
  display.setCursor(0, 0 + SCREEN_OFFSET);
  display.println("Connecting...");

  initWiFi();
  delay(1000);
  display.fillScreen(AUDI_RED);
  pinMode(SPEED_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SPEED_SENSOR_PIN), pulseCounter, RISING);
}

void loop() {
  // Empfangen und Verarbeiten der Daten
  //display.fillScreen(AUDI_RED);

  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime >= 1000) {
    float timeElapsed = (float)(currentTime - lastPulseTime) / 1000.0;  // Zeit in Sekunden
    float distance_km = (float)pulseCount / (float)K_FACTOR; // Entfernung in Kilometern
    speed_kmh = (distance_km / timeElapsed * 3600.0) / 10.0;
    
    lastPulseTime = currentTime;
    pulseCount = 0;
  }

  
  while (true) {
    int packetSize = Udp.parsePacket();

    if (packetSize) {
      char packetBuffer[255];
      int len = Udp.read(packetBuffer, 255);

      if (len > 0) {
        packetBuffer[len] = 0;
        String msg = String(packetBuffer);

        // Daten verarbeiten und auf dem Display anzeigen
        //display.fillScreen(AUDI_RED);
        display.setTextSize(3);
        display.setTextColor(AUDI_RED);
        //display.setCursor(0, 0 + SCREEN_OFFSET);

        display.fillRect(100, 0 + SCREEN_OFFSET, 280 - 100, 22, AUDI_RED);
        display.fillRect(0 + 1, 25 + SCREEN_OFFSET + 1, 280 - 2, 22 - 2, AUDI_RED);

        display.fillRect(120, 50 + SCREEN_OFFSET, 280 - 120, 22, AUDI_RED);
        display.fillRect(0 + 1, 75 + SCREEN_OFFSET + 1, 280 - 2, 22 - 2, AUDI_RED);

        display.setTextColor(AUDI_HIGHLIGHTED_RED);

        // JSON-Daten parsen und anzeigen
        DynamicJsonDocument data(255);
        DeserializationError error = deserializeJson(data, msg);

        if (error) {
          display.fillScreen(AUDI_RED);
          display.println("Error parsing JSON!");
        } else {
          String actuatorString = "Act.: " + String(data["sensor1"].as<const char*>()) + "mA";
          display.setCursor(0, 0 + SCREEN_OFFSET);
          display.println(actuatorString);
          drawValueBar(0, 25 + SCREEN_OFFSET, 280, 22, v1Min, v1Max, float(data["sensor1"]), v1Vline, false);

          String lambdaString = "Lambda: " + String(data["sensor2"].as<const char*>());
          display.setCursor(0, 50 + SCREEN_OFFSET);
          display.println(lambdaString);
          drawValueBar(0, 75 + SCREEN_OFFSET, 280, 22, v2Min, v2Max, float(data["sensor2"]), v2Vline, false);
        }

        display.fillRect(0, 125 + SCREEN_OFFSET, 280, 22, AUDI_RED);
        display.setCursor(0, 125 + SCREEN_OFFSET);
        display.print(speed_kmh, 0);
        display.println(F(" km/h"));

        break;
      }
    }
  }

  delay(150);
}
