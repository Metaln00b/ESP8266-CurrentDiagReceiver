#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncElegantOTA.h>
#include <WiFiUdp.h>

const char* ssid = "CurrentDiagReceiver";
const char* pass = "123456789";

AsyncWebServer server(80);

constexpr uint16_t PORT = 8266;
char packetBuffer[255];

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

  IPAddress apIP(192, 168, 4, 100);

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
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
  {
    ; // Needed for native USB port only
  }

  initWiFi();


  AsyncElegantOTA.begin(&server);

  server.begin();

  Udp.begin(PORT);
}

void loop() {
  int packetSize = Udp.parsePacket();

  Serial.print(" Received packet from : ");
  Serial.println(Udp.remoteIP());
  
  Serial.print(" Size : ");
  Serial.println(packetSize);
  
  Serial.print(" Data : ");
  if (packetSize) {
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    Serial.print(packetBuffer);
  }
  Serial.println("\n");
  delay(500);

  Serial.print("[Client Connected] ");
  Serial.println(WiFi.localIP());
}