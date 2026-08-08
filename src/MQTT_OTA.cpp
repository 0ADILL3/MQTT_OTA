#include "MQTT_OTA.h"

MQTT_OTA::MQTT_OTA(PubSubClient &client) : MQTT_Client_(client) {}

void MQTT_OTA::begin(const char *topic, uint16_t timeout, uint16_t size)
{
  topic_ = topic;
  timeout_ = timeout;
  size_ = size;

  MQTT_Client_.setBufferSize(size_);

  snprintf(subscribe_topic_, sizeof(subscribe_topic_), "%s/MQTT_OTA/Publisher/#", topic_);
  snprintf(status_topic_, sizeof(status_topic_), "%s/MQTT_OTA/Subscriber/MQTT_OTA_status", topic_);

  prefs_.begin("firmware_ver", true);
  if (prefs_.isKey("version")) {prefs_.getString("version", firmware_version_, sizeof(firmware_version_));}
  prefs_.end();
}

void MQTT_OTA::handle()
{
  if ((MQTT_Client_status_ != MQTT_Client_.connected()) && (MQTT_Client_.connected() == true))
  {
    Serial.println();
    Serial.printf("[MQTT_OTA] MQTT_OTA Subscribe to topic: %s\n", topic_);
    MQTT_Client_.subscribe(subscribe_topic_);
  }
  MQTT_Client_status_ = MQTT_Client_.connected();

  if (Update.isRunning())
  {
    if (millis() - last_time_ > timeout_)
    {
      MQTT_Client_.publish(status_topic_, "ABORTED: TIMEOUT");
      Serial.println();
      Serial.println("[MQTT_OTA] OTA Timeout! Aborting");
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

  if (strstr(topic, "/MQTT_OTA/Publisher/start") != NULL)
  {
    char payload_start[256];
    size_t new_length = min(length, (unsigned int)(sizeof(payload_start)-1));
    memcpy(payload_start, payload, new_length);
    payload_start[new_length] = '\0'; 
    
    int file_size = 0;
    uint16_t chunk_size = 0;
    char hash_MD5[33] = {0};
    
    JsonDocument doc;
    DeserializationError json_error = deserializeJson(doc, payload_start);

    if (!json_error)
    {
      file_size = doc["file_size"];
      chunk_size = doc["chunk_size"];
      strlcpy(hash_MD5, doc["hash_MD5"] | "", sizeof(hash_MD5));
      strlcpy(new_firmware_version_, doc["version"] | "", sizeof(new_firmware_version_));
      
      if (file_size == 0 || chunk_size == 0 || strlen(hash_MD5) == 0 || strlen(new_firmware_version_) == 0)
      {
        MQTT_Client_.publish(status_topic_, "ABORTED: MISSING PARAMETERS");
        Serial.println();
        Serial.println("[MQTT_OTA] OTA Dibatalkan: Parameter JSON tidak lengkap!");
        Update.abort();
        return;
      }

      if ((chunk_size + 256) > size_) 
      {
        MQTT_Client_.publish(status_topic_, "ABORTED: OVERSIZED CHUNK");
        Serial.println();
        Serial.println("[MQTT_OTA] OTA Dibatalkan: Chunk dari publisher melebihi kapasitas buffer");
        Update.abort();
        return;
      }
      
      Update.setMD5(hash_MD5);
      
      if (Update.begin(file_size))
      {
        MQTT_Client_.publish(status_topic_, "BEGIN ACKNOWLEDGMENT: OK");
        Serial.println();
        Serial.println("[MQTT_OTA] Menerima data OTA...");
      }
      else
      {
        MQTT_Client_.publish(status_topic_, "BEGIN ACKNOWLEDGMENT: FAILED");
        Serial.println();
        Serial.printf("[MQTT_OTA] Update.begin() gagal. Error=%d\n", Update.getError());
        Update.abort();
        return;
      }
    }
    else
    {
      MQTT_Client_.publish(status_topic_, "ABORTED: INVALID FORMAT");
      Serial.println();
      Serial.print("[MQTT_OTA] Format payload START tidak valid (JSON Error): ");
      Serial.println(json_error.c_str());
      Update.abort();
      return;
    }
  }
  else if (strstr(topic, "/MQTT_OTA/Publisher/data") != NULL)
  {
    if (Update.isRunning()) {Update.write(payload, length);}
  }
  else if (strstr(topic, "/MQTT_OTA/Publisher/end") != NULL)
  {
    Serial.println();
    Serial.println("[MQTT_OTA] Menerima sinyal END...");
    
    if (Update.end(true))
    {
      MQTT_Client_.publish(status_topic_, "FIRMWARE VALIDATION: VALID");
      Serial.println();
      Serial.println("[MQTT_OTA] OTA Selesai dan MD5 Valid!");
      
      Serial.printf("[MQTT_OTA] Saving Firmware Version: %s\n", new_firmware_version_);
      prefs_.begin("firmware_ver", false);
      prefs_.putString("version", new_firmware_version_);
      prefs_.end();

      Serial.println("[MQTT_OTA] Restarting...");
      Serial.println();
      ESP.restart();
    }
    else
    {
      MQTT_Client_.publish(status_topic_, "FIRMWARE VALIDATION: INVALID");
      Serial.println();
      Serial.print("[MQTT_OTA] OTA Gagal (Mungkin MD5 Invalid)! Kode Error: ");
      Serial.println(Update.getError());
      Update.abort();
      return;
    }
  }
}

String MQTT_OTA::get_firmware_version() {return String(firmware_version_);}