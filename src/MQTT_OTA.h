#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>

class MQTT_OTA
{
  private:
    PubSubClient &_MQTT_Client;
    Preferences _prefs;
    
    const char *_MQTT_OTA_Topic;
    uint16_t _MQTT_OTA_timeout;
    uint16_t _MQTT_OTA_size;
    
    bool _MQTT_Client_status = false;
    char _firmware_version[17] = "v1.0";
    char _new_firmware_version[17] = {0};
    unsigned long _last_time = 0;
  
  public:
    MQTT_OTA(
      PubSubClient &client,
      const char *topic,
      uint16_t timeout = 10000,
      uint16_t size = 8192
    );

    void begin();
    void handle();
    void MQTT_OTA_callback(char *topic, byte *payload, unsigned int length);
    String get_firmware_version();
};