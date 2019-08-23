//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

/************ WIFI, OTA and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
//#define WIFI_SSID "" //enter your WIFI SSID
//#define WIFI_PASSWORD "" //enter your WIFI Password
//#define MQTT_SERVER "" // Enter your MQTT server address or IP.
//#define MQTT_USER "" //enter your MQTT username
//#define MQTT_PASSWORD "" //enter your password
#define MQTT_DEVICE "watchdog-test" // Enter your MQTT device
#define MQTT_PORT 8883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.0"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/
#define MQTT_VERSION_PUB "sensor/xxx/version"
#define MQTT_COMPILE_PUB "sensor/xxx/compile"
#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_HEARTBEAT_PUB "mqtt/template/heartbeat"

#define WATCHDOG_PIN 5  //  D1

#include <ESP8266SSDP.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "credentials.h" // Place credentials for wifi and mqtt in this file
#include "certificates.h" // Place certificates for mqtt in this file
 
Ticker ticker_fw;

bool readyForFwUpdate = false;

WiFiClientSecure espClient;
PubSubClient client(espClient);

#include "common.h"
 
void setup() {
 
  Serial.begin(115200);

  pinMode(WATCHDOG_PIN, OUTPUT); 

  setup_wifi();

  client.setServer(MQTT_SERVER, MQTT_PORT); //1883 is the port number you have forwared for mqtt messages. You will need to change this if you've used a different port 
  client.setCallback(callback); //callback is the function that gets called for a topic sub

  ticker_fw.attach_ms(FW_UPDATE_INTERVAL_SEC * 1000, fwTicker);

  checkForUpdates();
  resetWatchdog();
}

int count=0;

void loop() {
  
  //If MQTT client can't connect to broker, then reconnect
  if (!client.connected()) {
    reconnect();
  }

  if(readyForFwUpdate) {
    readyForFwUpdate = false;
    checkForUpdates();
  }

  client.loop(); //the mqtt function that processes MQTT messages

  my_delay(1000);
  count++;
  Serial.println(count);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String strTopic;
  //if the 'garage/button' topic has a payload "OPEN", then 'click' the relay
  payload[length] = '\0';
  strTopic = String((char*)topic);
  if (strTopic == MQTT_HEARTBEAT_TOPIC) {
    resetWatchdog();
    Serial.println("Heartbeat received");
  }
}
