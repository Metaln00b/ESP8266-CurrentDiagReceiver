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
#include "audi_logo.h"

TFT_eSPI display = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&display);

const char *ssid = "CurrentDiagReceiver";
const char *pass = "123456789";

AsyncWebServer server(80);

#define SCREEN_HEIGHT 240
#define SCREEN_OFFSET 25

#ifndef DESKTOP_DISPLAY
#define SCREEN_WIDTH 280
#else
#define BL_PIN 5
#define SCREEN_WIDTH 240
#endif

#define SPEED_SENSOR_PIN D2
#define K_FACTOR 4127.0

volatile unsigned int pulseCount = 0;
unsigned long lastPulseTime = 0;
unsigned long lastUdpProcessTime = 0;
unsigned long udpProcessInterval = 0;
float speed_kmh = 0;

#define AUDI_RED 0x4800             // RGB565
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
    int cnt = 1;

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

    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.disconnect();
        WiFi.begin(F("CurrentDiag"), pass);
        Serial.println();
        Serial.print(F("Wait for WiFi"));

        while (WiFi.status() != WL_CONNECTED)
        {
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

void drawValueBar(int x, int y, int width, int height, float valueMin, float valueMax, float value, float vlineValue, bool showNumbers)
{
    float roundValue = min(max(valueMin, value), valueMax);
    float valueRange = valueMax - valueMin;
    float pxPerValue = (width - 4) / valueRange;
    int pxPositionVline = int(pxPerValue * (vlineValue - valueMin) + 2);
    int pxPositionValue = int(pxPerValue * (roundValue - valueMin) + 2);
    int start = min(pxPositionValue, pxPositionVline);
    int end = max(pxPositionValue, pxPositionVline);

    display.drawRect(x, y, width, height, AUDI_HIGHLIGHTED_RED);
    display.fillRect(x + start, y + 2, end - start, height - 4, AUDI_HIGHLIGHTED_RED);

    if (height >= 15 && showNumbers)
    {
        char roundValueStr[10];
        dtostrf(roundValue, 5, 2, roundValueStr);

        if (value >= vlineValue)
        {
            display.setTextSize(3);
            display.setTextColor(AUDI_HIGHLIGHTED_RED);
            display.setCursor(int(width / 9), y + 5);
            display.println(roundValueStr);
        }
        else
        {
            display.setTextSize(3);
            display.setTextColor(AUDI_HIGHLIGHTED_RED);
            display.setCursor(int(width / 1.5), y + 5);
            display.println(roundValueStr);
        }
    }

    display.drawFastVLine(pxPositionVline, y - 1, height + 2, AUDI_HIGHLIGHTED_RED);
}

void drawValueBarSprite(int x, int y, int width, int height, float valueMin, float valueMax, float value, float vlineValue, bool showNumbers)
{
    float roundValue = min(max(valueMin, value), valueMax);
    float valueRange = valueMax - valueMin;
    float pxPerValue = (width - 4) / valueRange;
    int pxPositionVline = int(pxPerValue * (vlineValue - valueMin) + 2);
    int pxPositionValue = int(pxPerValue * (roundValue - valueMin) + 2);
    int start = min(pxPositionValue, pxPositionVline);
    int end = max(pxPositionValue, pxPositionVline);

    spr.drawRect(x, y, width, height, AUDI_HIGHLIGHTED_RED);
    spr.fillRect(x + start, y + 2, end - start, height - 4, AUDI_HIGHLIGHTED_RED);

    if (height >= 15 && showNumbers)
    {
        char roundValueStr[10];
        dtostrf(roundValue, 5, 2, roundValueStr);

        if (value >= vlineValue)
        {
            spr.setTextSize(3);
            spr.setTextColor(AUDI_HIGHLIGHTED_RED);
            spr.setCursor(int(width / 9), y + 5);
            spr.println(roundValueStr);
        }
        else
        {
            spr.setTextSize(3);
            spr.setTextColor(AUDI_HIGHLIGHTED_RED);
            spr.setCursor(int(width / 1.5), y + 5);
            spr.println(roundValueStr);
        }
    }

    spr.drawFastVLine(pxPositionVline, y - 1, height + 2, AUDI_HIGHLIGHTED_RED);
}

void IRAM_ATTR pulseCounter()
{
    pulseCount++;
}

void processUdpPackets()
{
    int packetSize = Udp.parsePacket();

    if (packetSize)
    {
        char packetBuffer[255];
        int len = Udp.read(packetBuffer, 255);

        if (len > 0)
        {
            packetBuffer[len] = '\0';
            const char *msg = packetBuffer;

            spr.setTextSize(2);
            spr.setTextColor(AUDI_HIGHLIGHTED_RED);

            // JSON-Daten parsen und anzeigen
            DynamicJsonDocument data(255);
            DeserializationError error = deserializeJson(data, msg);

            if (error)
            {
                display.fillScreen(AUDI_RED);
                display.println(F("Error parsing JSON!"));
            }
            else
            {
                float sensor1Value = atof(data["sensor1"]);
                float sensor2Value = atof(data["sensor2"]);

                char sensor1ValueStr[10];
                char sensor2ValueStr[10];
                dtostrf(sensor1Value, 6, 2, sensor1ValueStr);
                dtostrf(sensor2Value, 3, 2, sensor2ValueStr);

                // Actuator
                spr.fillSprite(AUDI_RED);

                spr.setCursor(0, 0);
                spr.print(F("Act.: "));
                spr.print(sensor1ValueStr);
                spr.println(F("mA"));
                drawValueBarSprite(0, 25, SCREEN_WIDTH, 22, v1Min, v1Max, sensor1Value, v1Vline, false);

                spr.pushSprite(0, 0 + SCREEN_OFFSET);

                // Lambda
                spr.fillSprite(AUDI_RED);

                spr.setCursor(0, 0);
                spr.print(F("Lambda: "));
                spr.println(sensor2ValueStr);
                drawValueBarSprite(0, 25, SCREEN_WIDTH, 22, v2Min, v2Max, sensor2Value, v2Vline, false);

                spr.pushSprite(0, 75 + SCREEN_OFFSET);
            }

            spr.fillSprite(AUDI_RED);

            char speed_kmhStr[10];
            dtostrf(speed_kmh, 5, 0, speed_kmhStr);
            spr.setTextSize(4);
            spr.setCursor(0, 0);
            spr.print(speed_kmhStr);
            spr.println(F(" km/h"));

            spr.pushSprite(0, 150 + SCREEN_OFFSET);
        }
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        ; // Needed for native USB port only
    }

    if (!LittleFS.begin())
    {
        Serial.println("LittleFS mount failed!");
        return;
    }

#ifdef DESKTOP_DISPLAY
    pinMode(BL_PIN, OUTPUT);
    digitalWrite(BL_PIN, LOW);
#endif

    display.init();
#ifndef DESKTOP_DISPLAY
    display.setRotation(1);
#else
    display.setRotation(0);
#endif

    display.setTextColor(AUDI_HIGHLIGHTED_RED);
    display.fillScreen(AUDI_RED);
    display.setTextSize(2);

#ifndef DESKTOP_DISPLAY
    display.drawBitmap(20, 0, audi_logo_bits, audi_logo_width, audi_logo_height, AUDI_HIGHLIGHTED_RED);
#else
    display.drawBitmap(0, 0, audi_logo_bits, audi_logo_width, audi_logo_height, AUDI_HIGHLIGHTED_RED);
#endif
    display.setCursor(0, 180 + SCREEN_OFFSET);
    display.println(F("Connecting..."));

    spr.setColorDepth(16);
    spr.createSprite(SCREEN_WIDTH, 48);

    initWiFi();
    delay(1000);
    display.fillScreen(AUDI_RED);

    pinMode(SPEED_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SPEED_SENSOR_PIN), pulseCounter, FALLING);
}

void loop()
{
    unsigned long currentTime = millis();

    if (currentTime - lastPulseTime >= 500)
    {
        detachInterrupt(digitalPinToInterrupt(SPEED_SENSOR_PIN));
        float timeElapsed = (float)(currentTime - lastPulseTime) / 1000.0; // Zeit in Sekunden
        float distance_km = (float)pulseCount / (float)K_FACTOR;           // Entfernung in Kilometern
        speed_kmh = (distance_km / timeElapsed * 3600.0);

        lastPulseTime = currentTime;
        pulseCount = 0;
        attachInterrupt(digitalPinToInterrupt(SPEED_SENSOR_PIN), pulseCounter, FALLING);
    }

    if (currentTime - lastUdpProcessTime >= udpProcessInterval)
    {
        processUdpPackets();
        lastUdpProcessTime = currentTime;
    }
    ElegantOTA.loop();

    yield();
}
