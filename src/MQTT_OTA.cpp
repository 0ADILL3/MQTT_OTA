#include "MQTT_OTA.h"

MQTT_OTA::MQTT_OTA(PubSubClient &client) : MQTT_Client_(client) {}

void MQTT_OTA::begin(const char *topic, uint16_t timeout, uint16_t size)
{
  timeout_ = timeout;
  size_ = size;
  
  strlcpy(topic_, topic, sizeof(topic_));

  size_t len = strlen(topic_);
  while(len > 0 && (topic_[len-1] == '#' || topic_[len-1] == '/')) 
  {
    topic_[len-1] = '\0';
    len--;
  }

  MQTT_Client_.setBufferSize(size_);

  snprintf(subscribe_topic_, sizeof(subscribe_topic_), "%s/MQTT_OTA/Publisher/#", topic_);
  snprintf(status_topic_, sizeof(status_topic_), "%s/MQTT_OTA/Subscriber/status", topic_);

  if (prefs_.begin("firmware_ver", true))
  {
    if (prefs_.isKey("version")) {prefs_.getString("version", firmware_version_, sizeof(firmware_version_));}
    prefs_.end();
  }
}

void MQTT_OTA::handle()
{
  if ((MQTT_Client_status_ != MQTT_Client_.connected()) && (MQTT_Client_.connected() == true))
  {
    MQTT_OTA_LOG_F("MQTT_OTA Subscribe to topic: %s\n", subscribe_topic_);
    MQTT_Client_.subscribe(subscribe_topic_);
  }
  MQTT_Client_status_ = MQTT_Client_.connected();

  if (Update.isRunning())
  {
    if (millis() - last_time_ > timeout_)
    {
      MQTT_Client_.publish(status_topic_, ABORTED_TIMEOUT);
      MQTT_OTA_LOG_F("OTA Timeout! Aborting\n");
      Update.abort();
    }
  }
  else
  {
    last_time_ = millis();
  }
}

void MQTT_OTA::MQTT_OTA_callback(char *topic, byte *payload, unsigned int length)
{
  last_time_ = millis();

  if (strstr(topic, BEGIN_TOPIC) != NULL)
  { 
    int file_size = 0;
    uint16_t chunk_size = 0;
    char hash_MD5[33] = {0};
    
    JsonDocument doc;
    DeserializationError json_error = deserializeJson(doc, payload, length);

    if (!json_error)
    {
      file_size = doc["file_size"];
      chunk_size = doc["chunk_size"];
      strlcpy(hash_MD5, doc["hash_MD5"] | "", sizeof(hash_MD5));
      strlcpy(new_firmware_version_, doc["version"] | "", sizeof(new_firmware_version_));
      
      if (file_size == 0 || chunk_size == 0 || strlen(hash_MD5) == 0 || strlen(new_firmware_version_) == 0)
      {
        MQTT_Client_.publish(status_topic_, ABORTED_INVALID_FORMAT);
        MQTT_OTA_LOG_F("OTA Dibatalkan: Parameter JSON tidak lengkap!\n");
        Update.abort();
        return;
      }

      if ((chunk_size + 256) > size_) 
      {
        MQTT_Client_.publish(status_topic_, ABORTED_OVERSIZED_CHUNK);
        MQTT_OTA_LOG_F("OTA Dibatalkan: Chunk dari publisher melebihi kapasitas buffer\n");
        Update.abort();
        return;
      }
      
      Update.setMD5(hash_MD5);
      
      if (Update.begin(file_size))
      {
        MQTT_Client_.publish(status_topic_, BEGIN_ACKNOWLEDGMENT_OK);
        MQTT_OTA_LOG_F("Menerima data OTA...");
      }
      else
      {
        MQTT_Client_.publish(status_topic_, BEGIN_ACKNOWLEDGMENT_FAILED);
        MQTT_OTA_LOG_F("Update.begin() gagal. Error=%d\n", Update.getError());
        Update.abort();
        return;
      }
    }
    else
    {
      MQTT_Client_.publish(status_topic_, ABORTED_INVALID_FORMAT);
      MQTT_OTA_LOG_F("Format payload begin tidak valid (JSON Error): %s\n", json_error.c_str());
      Update.abort();
      return;
    }
  }
  else if (strstr(topic, DATA_TOPIC) != NULL)
  {
    if (Update.isRunning()) {Update.write(payload, length);}
  }
  else if (strstr(topic, END_TOPIC) != NULL)
  {
    MQTT_OTA_LOG_F("Menerima sinyal END...");
    
    if (Update.end(true))
    {
      MQTT_Client_.publish(status_topic_, END_FIRMWARE_VALID);
      MQTT_OTA_LOG_F("OTA Selesai dan MD5 Valid!");
      
      MQTT_OTA_LOG_F("Saving Firmware Version: %s", new_firmware_version_);
      prefs_.begin("firmware_ver", false);
      prefs_.putString("version", new_firmware_version_);
      prefs_.end();

      for (int i=0; i<3000; i++) {MQTT_Client_.loop(); delay(1);}

      MQTT_OTA_LOG_F("Restarting...\n\n");
      ESP.restart();
    }
    else
    {
      MQTT_Client_.publish(status_topic_, END_FIRMWARE_INVALID);
      MQTT_OTA_LOG_F("OTA Gagal (Mungkin MD5 Invalid)! Kode Error: %d\n", Update.getError());
      Update.abort();
      return;
    }
  }
}

String MQTT_OTA::get_firmware_version() {return String(firmware_version_);}