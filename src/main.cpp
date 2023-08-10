#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncElegantOTA.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiGratuitous.h>
#include <SPI.h>

const char* ssid = "CurrentDiag";
const char* pass = "123456789";

AsyncWebServer server(80);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 280
#define TFT_CS D6//D8
#define TFT_RST -1//D0
#define TFT_DC D1
#define TFT_BL D2
#define TFT_MOSI D7
#define TFT_SCLK D5

#define AUDI_RED 0x3000
#define AUDI_HIGHLIGHTED_RED 0x9000

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
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
  /* Serial.println("Setting soft-AP ... ");
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
  Serial.println(IP); */
  if (WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();                                          // probably not necessary due to WiFi.status() != WL_CONNECTED
    WiFi.begin(ssid, pass);                                 // reconnect to the Network
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
  display.setTextSize(1);
  display.setTextColor(AUDI_HIGHLIGHTED_RED);
  display.setCursor(0, 0);
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
      display.setTextSize(1);
      display.setTextColor(AUDI_HIGHLIGHTED_RED);
      display.setCursor(int(width / 9), y + 5);
      display.println(String(roundValue, 2));
    } else {
      display.setTextSize(1);
      display.setTextColor(AUDI_HIGHLIGHTED_RED);
      display.setCursor(int(width / 1.5), y + 5);
      display.println(String(roundValue, 2));
    }
  }

  display.drawFastVLine(pxPositionVline, y - 1, height + 2, AUDI_HIGHLIGHTED_RED);
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
  {
    ; // Needed for native USB port only
  }

  analogWrite(TFT_BL, 255);
  
  display.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  display.setSPISpeed(40000000);
  display.setRotation(3);
  display.setTextColor(AUDI_HIGHLIGHTED_RED);
  display.fillScreen(AUDI_RED);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting...");

  initWiFi();
  delay(1000);
}

void loop() {
  // Empfangen und Verarbeiten der Daten
  display.fillScreen(AUDI_RED);

  while (true) {
    int packetSize = Udp.parsePacket();

    if (packetSize) {
      char packetBuffer[255];
      int len = Udp.read(packetBuffer, 255);

      if (len > 0) {
        packetBuffer[len] = 0;
        String msg = String(packetBuffer);

        // Daten verarbeiten und auf dem Display anzeigen
        display.fillScreen(AUDI_RED);
        display.setTextSize(1);
        display.setTextColor(AUDI_HIGHLIGHTED_RED);
        display.setCursor(0, 0);

        // JSON-Daten parsen und anzeigen
        DynamicJsonDocument data(255);
        DeserializationError error = deserializeJson(data, msg);

        if (error) {
          display.println("Error parsing JSON!");
        } else {
          String actuatorString = "Actuator: " + String(data["sensor1"].as<const char*>()) + "mA";
          display.setCursor(0, 0);
          display.println(actuatorString);
          drawValueBar(0, 10, 128, 12, v1Min, v1Max, float(data["sensor1"]), v1Vline, false);

          String lambdaString = "Lambda: " + String(data["sensor2"].as<const char*>());
          display.setCursor(0, 26);
          display.println(lambdaString);
          drawValueBar(0, 36, 128, 12, v2Min, v2Max, float(data["sensor2"]), v2Vline, false);
        }

        break;
      }
    } else {
      // Fehler beim Empfangen des Pakets
      /* display.fillScreen(AUDI_RED);
      display.setTextSize(1);
      display.setTextColor(AUDI_HIGHLIGHTED_RED);
      display.setCursor(0, 0);
      display.println("Error receiving packet!");
      delay(EXDURATION * 1000); */
    }
  }

  delay(500);
}
