#include <ESP8266SSDP.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "credentials.h" // Place credentials for wifi and mqtt in this file
 
//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

/************ WIFI, OTA and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
//#define WIFI_SSID "" //enter your WIFI SSID
//#define WIFI_PASSWORD "" //enter your WIFI Password
//#define MQTT_SERVER "" // Enter your MQTT server address or IP.
//#define MQTT_DEVICE "" // Enter your MQTT device
//#define MQTT_USER "" //enter your MQTT username
//#define MQTT_PASSWORD "" //enter your password
#define MQTT_PORT 1883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define WATCHDOG_UPDATE_INTERVAL_SEC 1
#define WATCHDOG_RESET_INTERVAL_SEC 30
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.0"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/
#define MQTT_VERSION_PUB "sensor/garage-double/version"
#define MQTT_COMPILE_PUB "sensor/garage-double/compile"

volatile int watchDogCount = 0;

Ticker ticker_fw, ticker_watchdog;

bool readyForFwUpdate = false;

WiFiClient espClient;
 
//Initialize MQTT
PubSubClient client(espClient);
 
void setup() {
 
  Serial.begin(115200);

  setup_wifi();

  client.setServer(MQTT_SERVER, MQTT_PORT); //1883 is the port number you have forwared for mqtt messages. You will need to change this if you've used a different port 

  ticker_fw.attach_ms(FW_UPDATE_INTERVAL_SEC * 1000, fwTicker);
  ticker_watchdog.attach_ms(WATCHDOG_UPDATE_INTERVAL_SEC * 1000, watchdogTicker);

  checkForUpdates();
}

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
  watchDogCount = 0;
}
 
void setup_wifi() {
  int count = 0;
  delay(50);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MQTT_DEVICE);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    count++;
    if(count > 50) {
      WiFiManager wifiManager;
      wifiManager.resetSettings();
      wifiManager.autoConnect();
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
}

void reconnect() {
  //Reconnect to Wifi and to MQTT. If Wifi is already connected, then autoconnect doesn't do anything.
  Serial.print("Attempting MQTT connection...");
  if (client.connect(MQTT_DEVICE, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("connected");
    String firmwareVer = String("Firmware Version: ") + String(FIRMWARE_VERSION);
    String compileDate = String("Build Date: ") + String(compile_date);
    client.publish(MQTT_VERSION_PUB, firmwareVer.c_str(), true);
    client.publish(MQTT_COMPILE_PUB, compileDate.c_str(), true);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }
}

// FW update ticker
void fwTicker() {
  readyForFwUpdate = true;
}

// Watchdog update ticker
void watchdogTicker() {
  watchDogCount++;
  if(watchDogCount >= WATCHDOG_RESET_INTERVAL_SEC) {
    Serial.println("Reset system");
    ESP.restart();  
  }
}

String WiFi_macAddressOf(IPAddress aIp) {
  if (aIp == WiFi.localIP())
    return WiFi.macAddress();

  if (aIp == WiFi.softAPIP())
    return WiFi.softAPmacAddress();

  return String("00-00-00-00-00-00");
}

void checkForUpdates() {

  String clientMAC = WiFi_macAddressOf(espClient.localIP());

  Serial.print("MAC: ");
  Serial.println(clientMAC);
  clientMAC.replace(":", "-");
  String filename = clientMAC.substring(9);
  String firmware_URL = String(UPDATE_SERVER) + filename + String(FIRMWARE_VERSION);
  String current_firmware_version_URL = String(UPDATE_SERVER) + filename + String("-current_version");

  HTTPClient http;

  http.begin(current_firmware_version_URL);
  int httpCode = http.GET();
  
  if ( httpCode == 200 ) {

    String newFirmwareVersion = http.getString();
    newFirmwareVersion.trim();
    
    Serial.print( "Current firmware version: " );
    Serial.println( FIRMWARE_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFirmwareVersion );
    
    if(newFirmwareVersion.substring(1).toFloat() > String(FIRMWARE_VERSION).substring(1).toFloat()) {
      Serial.println( "Preparing to update" );
      String new_firmware_URL = String(UPDATE_SERVER) + filename + newFirmwareVersion + ".bin";
      Serial.println(new_firmware_URL);
      t_httpUpdate_return ret = ESPhttpUpdate.update( new_firmware_URL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
         break;
      }
    }
    else {
      Serial.println("Already on latest firmware");  
    }
  }
  else {
    Serial.print("GET RC: ");
    Serial.println(httpCode);
  }
}
