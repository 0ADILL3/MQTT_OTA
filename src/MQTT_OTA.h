#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define DEBUG_MQTT_OTA 0

#if DEBUG_MQTT_OTA
  #define MQTT_OTA_LOG(...) do {Serial.printf(__VA_ARGS__);} while (0)
#else
  #define MQTT_OTA_LOG(...) do {} while (0)
#endif

#define FIRMWARE_VERSION_NAME_MAX_LEN 16
#define SUBSCRIBE_TOPIC_MAX_LEN       128
#define STATUS_TOPIC_MAX_LEN          128

#define BEGIN_TOPIC                   "MQTT_OTA/Publisher/begin"
#define DATA_TOPIC                    "MQTT_OTA/Publisher/data"
#define END_TOPIC                     "MQTT_OTA/Publisher/end"

#define BEGIN_ACKNOWLEDGMENT_OK       "BEGIN: ACKNOWLEDGMENT OK"
#define BEGIN_ACKNOWLEDGMENT_FAILED   "BEGIN: ACKNOWLEDGMENT FAILED"
#define END_FIRMWARE_VALID            "END: FIRMWARE VALID"
#define END_FIRMWARE_INVALID          "END: FIRMWARE INVALID"
#define ABORTED_TIMEOUT               "ABORTED: TIMEOUT"
#define ABORTED_OVERSIZED_CHUNK       "ABORTED: OVERSIZED CHUNK"
#define ABORTED_INVALID_FORMAT        "ABORTED: INVALID FORMAT"

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
    char subscribe_topic_[SUBSCRIBE_TOPIC_MAX_LEN];
    char status_topic_[STATUS_TOPIC_MAX_LEN];
    unsigned long last_time_ = 0;
  
  public:
    // Initialize MQTT OTA with PubSubClient reference
    MQTT_OTA(PubSubClient &client);

    // Initialize MQTT OTA settings and parameters
    void begin(const char *topic, uint16_t timeout = 30000, uint16_t size = 8192);
    // Handle MQTT OTA process and timeout
    void handle();
    // Process MQTT OTA messages
    void MQTT_OTA_callback(char *topic, byte *payload, unsigned int length);
    // Get current firmware version
    String get_firmware_version();
};