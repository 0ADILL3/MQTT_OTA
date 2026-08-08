#include <WiFi.h>
#include <PubSubClient.h>

#include "MQTT_OTA.h" 

// Konfigurasi WiFi & MQTT
const char *SSID           = "WIFI_SSID";
const char *Password       = "WIFI_PASSWORD";

const char *MQTT_Server    = "192.168.1.100";        
const char *MQTT_Port      = "1883";                 
const char *MQTT_Username  = "";                     
const char *MQTT_Password  = "";                     
const char *MQTT_Client_ID = "MQTT_OTA_Client";      
const char *MQTT_Topic     = "device/node_1";        

WiFiClient ESP32_Client;
PubSubClient Client_MQTT(ESP32_Client);
MQTT_OTA MQTT_OTA_Client(Client_MQTT);

void setup_WiFi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
  WiFi.begin(SSID, Password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void callback(char *topic, byte *payload, unsigned int length)
{
  MQTT_OTA_Client.MQTT_OTA_callback(topic, payload, length);
}

void connect_MQTT()
{
  if (Client_MQTT.connected()) {return;}
  
  if (WiFi.status() != WL_CONNECTED) { return; }
  
  Serial.print("\nConnecting to MQTT...");
  if (Client_MQTT.connect(MQTT_Client_ID, MQTT_Username, MQTT_Password))
  {
    Serial.println("OK");
    // Library akan melakukan auto-subscribe ke target topik di dalam fungsi handle()
  }
  else
  {
    Serial.print("Failed. rc=");
    Serial.println(Client_MQTT.state());
  }
}

void setup() {
  Serial.begin(115200);
  setup_WiFi();
  
  Client_MQTT.setServer(MQTT_Server, 1883);
  Client_MQTT.setCallback(callback);
  
  // Ini otomatis mengeksekusi inisialisasi Preferences dan setBufferSize(8192)
  MQTT_OTA_Client.begin(MQTT_Topic); 
}

void loop() {
  connect_MQTT();
  Client_MQTT.loop();
  
  // Menjalankan pengecekan timeout & auto-subscribe topik secara paralel
  MQTT_OTA_Client.handle();
}