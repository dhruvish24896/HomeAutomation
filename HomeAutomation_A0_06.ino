//**************** Inlcude Files *************************
#include "Webserver.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//************ Global Defines ********************************
#define ONLINE_WORK_MODE              (1)
#define OFFLINE_WORK_MODE             (2)
#define UDP_PORT                      (4210)
#define UDP_INCOMMING_PACKET_SIZE     (255)


//************************************ Global Varialbles *************************
bool connected_to_router = 0;
uint8_t mqtt_not_connected_cntr = 0;
uint8_t work_mode = 0;
const int Erasing_button = 0;
long lastMsg = 0;
char msg[50];
int value = 0;
char incomingPacket[UDP_INCOMMING_PACKET_SIZE];
static long previousMillis = 0;


//************************************ Global Objects  *************************
credentials Credentials;
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP UDP_Server;



//Provide credentials for your ESP server
char* esp_ssid = "MainHub";
char* esp_pass = "123456789";
const char* mqtt_server = "broker.mqtt-dashboard.com";


void setup()
{
  Serial.begin(115200);
  randomSeed(micros());

  pinMode(Erasing_button, INPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  for (uint8_t t = 4; t > 0; t--)
  {
    Serial.println(t);
    delay(1000);
  }

  // Press and hold the button to erase all the credentials
  if (digitalRead(Erasing_button) == LOW)
  {
    Credentials.Erase_eeprom();
  }

  Credentials.EEPROM_Config();

  if (Credentials.credentials_get())
  {
    connected_to_router = 1;  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer


    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready");
  }
  else
  {
    Credentials.setupAP(esp_ssid, esp_pass);
    connected_to_router = 0;
  }
  if (connected_to_router)
  {
    client.setServer(mqtt_server, 1883);
    client.setCallback(mqtt_server_handler);
    if (!client.connected())
    {
      if (reconnect())
      {
        work_mode = ONLINE_WORK_MODE;
        Serial.println("online work mode set...");
      }
      else
      {
        WiFi.config(IPAddress(192, 168, 43, 60), IPAddress(192, 168, 43, 60), IPAddress(255, 255, 255, 0));
        UDP_Server.begin(UDP_PORT);
        Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), UDP_PORT);
        work_mode = OFFLINE_WORK_MODE;
        Serial.println("offline work mode set...");
      }
    }
  }
}


void loop()
{
  ArduinoOTA.handle();
  led_blink();
  if (connected_to_router)
  {
    if (work_mode == ONLINE_WORK_MODE)
    {
      if (!client.connected())
      {
        if (reconnect())
        {
          work_mode = ONLINE_WORK_MODE;
          Serial.println("online work mode set...");
        }
        else
        {
          work_mode = OFFLINE_WORK_MODE;
          WiFi.config(IPAddress(192, 168, 43, 60), IPAddress(192, 168, 43, 60), IPAddress(255, 255, 255, 0));
          UDP_Server.begin(UDP_PORT);
          Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), UDP_PORT);
          Serial.println("offline work mode set...");
        }
      }
      client.loop();
    }
    else if (work_mode == OFFLINE_WORK_MODE)
    {
      udp_server_handler();
      delay(100);
    }
  }
  else
  {
    Credentials.server_loops();
  }
}

void Device_Handler(String RCV_Packet)
{
  if (RCV_Packet == "ROOM_1/LAMP_1")
  {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void udp_server_handler(void)
{
  int packetSize = UDP_Server.parsePacket();
  if (packetSize)
  {
    // receive incoming UDP packets
    //    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, UDP_Server.remoteIP().toString().c_str(), UDP_Server.remotePort());
    int len = UDP_Server.read(incomingPacket, 255);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }
    Serial.printf("UDP packet contents: %s\n", incomingPacket);
    String RCV_Packet = incomingPacket;
    Device_Handler(RCV_Packet);
  }
}


void mqtt_server_handler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String mqtt_cmd = "";
  for (int i = 0; i < length; i++)
  {
    mqtt_cmd += ((char)payload[i]);
  }
  Device_Handler(mqtt_cmd);
}


boolean reconnect()
{
  boolean ret_val = false;
  // Loop until we're reconnected
  while ((!client.connected()) && (mqtt_not_connected_cntr < 5))
  {
    mqtt_not_connected_cntr++;
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);


    //     Attempt to connect
    //    if(client.connect(clientId,userName,passWord)) //put your clientId/userName/passWord here
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      ret_val = true;
      // Once connected, publish an announcement...
      //      client.publish("OUTTOPIC", "hello world");
      // ... and resubscribe
      client.subscribe("INTOPIC");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      ret_val = false;
      delay(5000);
    }
  }
  return ret_val;
}

void UDP_Send(char* msg)
{
  UDP_Server.beginPacket(UDP_Server.remoteIP(), UDP_Server.remotePort());
  UDP_Server.write(msg);
  UDP_Server.endPacket();
}

void led_blink(void)
{
  unsigned long currentMillis = millis();
  if (work_mode == ONLINE_WORK_MODE)
  {
    if (currentMillis - previousMillis > 1000) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
  if (work_mode == OFFLINE_WORK_MODE)
  {
    if (currentMillis - previousMillis > 500) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
  if (connected_to_router == 0)
  {
    if (currentMillis - previousMillis > 200) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
}
