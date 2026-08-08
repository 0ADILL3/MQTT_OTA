#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define FIRMWARE_VERSION_NAME_MAX_LEN    16
#define MQTT_OTA_SUBSCRIBE_TOPIC_MAX_LEN 128
#define MQTT_OTA_STATUS_TOPIC_MAX_LEN    128

class MQTT_OTA
{
  private:
    PubSubClient &MQTT_Client_;
    Preferences prefs_;
    
    const char *topic_;
    uint16_t timeout_;
    uint16_t size_;
    
    bool MQTT_Client_status_ = false;
    char firmware_version_[FIRMWARE_VERSION_NAME_MAX_LEN + 1] = "v1.0.0";
    char new_firmware_version_[FIRMWARE_VERSION_NAME_MAX_LEN + 1] = {0};
    char subscribe_topic_[MQTT_OTA_SUBSCRIBE_TOPIC_MAX_LEN];
    char status_topic_[MQTT_OTA_STATUS_TOPIC_MAX_LEN];
    unsigned long last_time_ = 0;
  
  public:
    // Initialize MQTT OTA with PubSubClient reference
    MQTT_OTA(PubSubClient &client);

    // Initialize MQTT OTA settings and parameters
    void begin(const char *topic, uint16_t timeout = 10000, uint16_t size = 8192);
    // Handle MQTT OTA process and timeout
    void handle();
    // Process MQTT OTA messages
    void MQTT_OTA_callback(char *topic, byte *payload, unsigned int length);
    // Get current firmware version
    String get_firmware_version();
};