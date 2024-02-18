#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiGratuitous.h>
#include <TFT_eSPI.h>
#include <SPI.h>

#define DRAW_DIGITS

#ifdef DRAW_DIGITS
  #include "NotoSans_Bold.h"
  #include "OpenFontRender.h"
  #define TTF_FONT NotoSans_Bold
#endif

TFT_eSPI display = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&display);

#ifdef DRAW_DIGITS
OpenFontRender ofr;
#endif

const char* ssid = "CurrentDiagReceiver";
const char* pass = "123456789";

AsyncWebServer server(80);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define SCREEN_OFFSET 50
#define BL_PIN 5

unsigned long lastUdpProcessTime = 0;
unsigned long udpProcessInterval = 0;

#define AUDI_RED 0x0800 // RGB565
#define AUDI_HIGHLIGHTED_RED 0xF980 // RGB565

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
  
  Serial.print(F("Connected clients: "));
  Serial.println(number_client);

  while (stat_info != NULL)
  {
      IPaddress = &stat_info->ip;
      address = IPaddress->addr;

      Serial.print(cnt);
      Serial.print(F(": IP: "));
      Serial.print((address));
      Serial.print(F(" MAC: "));

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
  Serial.println(F("Setting soft-AP ... "));
  wifi_set_event_handler_cb(eventCb);

  IPAddress apIP(192, 168, 4, 1);

  if (WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)))
    Serial.println(F("softAPConfig: True"));
  else
    Serial.println(F("softAPConfig: False"));

  if (WiFi.softAP(ssid, pass, 8))
    Serial.println(F("softAP: True"));
  else
    Serial.println(F("softAP: False"));
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print(F("AP IP address: "));
  Serial.println(IP);

  if (WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();
    WiFi.begin(F("CurrentDiag"), pass);
    Serial.println();
    Serial.print(F("Wait for WiFi"));

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(F("."));
    }
    Serial.println();
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: ") + WiFi.localIP().toString());
  }

  ElegantOTA.begin(&server);

  server.begin();

  Udp.begin(PORT);
  experimental::ESP8266WiFiGratuitous::stationKeepAliveSetIntervalMs(5000);
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
    char roundValueStr[10];
    dtostrf(roundValue, 5, 2, roundValueStr);
    
    if (value >= vlineValue) {
      display.setTextSize(3);
      display.setTextColor(AUDI_HIGHLIGHTED_RED);
      display.setCursor(int(width / 9), y + 5);
      display.println(roundValueStr);
    } else {
      display.setTextSize(3);
      display.setTextColor(AUDI_HIGHLIGHTED_RED);
      display.setCursor(int(width / 1.5), y + 5);
      display.println(roundValueStr);
    }
  }

  display.drawFastVLine(pxPositionVline, y - 1, height + 2, AUDI_HIGHLIGHTED_RED);
}

void processUdpPackets() {
  int packetSize = Udp.parsePacket();

  if (packetSize) {
    char packetBuffer[255];
    int len = Udp.read(packetBuffer, 255);

    if (len > 0) {
      packetBuffer[len] = '\0';
      const char* msg = packetBuffer;

      display.setTextSize(3);
      display.setTextColor(AUDI_RED);

      display.fillRect(90, 0 + SCREEN_OFFSET, 240 - 90, 22, AUDI_RED);
      display.fillRect(0 + 1, 25 + SCREEN_OFFSET + 1, 240 - 2, 22 - 2, AUDI_RED);

      display.fillRect(120, 50 + SCREEN_OFFSET, 240 - 120, 22, AUDI_RED);
      display.fillRect(0 + 1, 75 + SCREEN_OFFSET + 1, 240 - 2, 22 - 2, AUDI_RED);

      display.setTextColor(AUDI_HIGHLIGHTED_RED);

      // JSON-Daten parsen und anzeigen
      DynamicJsonDocument data(255);
      DeserializationError error = deserializeJson(data, msg);

      if (error) {
        display.fillScreen(AUDI_RED);
        display.println(F("Error parsing JSON!"));
      } else {
        float sensor1Value = atof(data["sensor1"]);
        float sensor2Value = atof(data["sensor2"]);

        char sensor1ValueStr[10];
        char sensor2ValueStr[10];
        dtostrf(sensor1Value, 6, 2, sensor1ValueStr);
        dtostrf(sensor2Value, 3, 2, sensor2ValueStr);

        display.setCursor(0, 0 + SCREEN_OFFSET);
        display.print(F("Act. "));
        display.print(sensor1ValueStr);
        display.println(F("mA"));
        drawValueBar(0, 25 + SCREEN_OFFSET, 240, 22, v1Min, v1Max, sensor1Value, v1Vline, false);

        display.setCursor(0, 50 + SCREEN_OFFSET);
        display.print(F("Lambda "));
        display.print(sensor2ValueStr);
        display.println();
        drawValueBar(0, 75 + SCREEN_OFFSET, 240, 22, v2Min, v2Max, sensor2Value, v2Vline, false);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
  {
    ; // Needed for native USB port only
  }

  display.init();

  display.setRotation(0);
  display.setTextColor(AUDI_HIGHLIGHTED_RED);
  display.fillScreen(AUDI_RED);
  display.setTextSize(3);
  display.setCursor(0, 0 + SCREEN_OFFSET);
  display.println(F("Connecting..."));

  pinMode(BL_PIN, OUTPUT);
  digitalWrite(BL_PIN, LOW);

  initWiFi();
  delay(1000);
  display.fillScreen(AUDI_RED);
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastUdpProcessTime >= udpProcessInterval) {
    processUdpPackets();
    lastUdpProcessTime = currentTime;
  }

  yield();
}
