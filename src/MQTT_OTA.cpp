#include "MQTT_OTA.h"

MQTT_OTA::MQTT_OTA(
  PubSubClient &client,
  const char *topic,
  uint16_t timeout,
  uint16_t size
) : 
  _MQTT_Client(client),
  _MQTT_OTA_Topic(topic),
  _MQTT_OTA_timeout(timeout),
  _MQTT_OTA_size(size)
{}

void MQTT_OTA::begin()
{
  _MQTT_Client.setBufferSize(_MQTT_OTA_size);

  _prefs.begin("firmware_ver", true);
  _prefs.getString("version", _firmware_version).toCharArray(_firmware_version, sizeof(_firmware_version));
  _prefs.end();
}

void MQTT_OTA::handle()
{
  if ((_MQTT_Client_status != _MQTT_Client.connected()) && (_MQTT_Client.connected() == true))
  {
    Serial.println();
    Serial.printf("[MQTT_OTA] MQTT_OTA Subscribe to topic: %s\n", _MQTT_OTA_Topic);
    _MQTT_Client.subscribe((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Publisher/#").c_str());
  }
  _MQTT_Client_status = _MQTT_Client.connected();

  if (Update.isRunning())
  {
    if (millis() - _last_time > _MQTT_OTA_timeout)
    {
      _MQTT_Client.publish((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Subscriber/MQTT_OTA_status").c_str(), "ABORTED: TIMEOUT");
      Serial.println();
      Serial.println("[MQTT_OTA] OTA Timeout! Aborting");
      Update.abort();
    }
  }
  else
  {
    _last_time = millis();
  }
}

void MQTT_OTA::MQTT_OTA_callback(char *topic, byte *payload, unsigned int length)
{
  _last_time = millis();

  if (strstr(topic, "/MQTT_OTA/Publisher/start") != NULL)
  {
    char payload_start[256];
    size_t new_length = min(length, (sizeof(payload_start)-1));
    memcpy(payload_start, payload, new_length);
    payload_start[new_length] = '\0'; 
    
    int file_size = 0;
    uint16_t chunk_size = 0;
    char hash_MD5[33] = {0};
    
    StaticJsonDocument<256> doc;
    DeserializationError json_error = deserializeJson(doc, payload_start);

    if (!json_error)
    {
      file_size = doc["file_size"];
      chunk_size = doc["chunk_size"];
      strlcpy(hash_MD5, doc["hash_MD5"] | "", sizeof(hash_MD5));
      strlcpy(_new_firmware_version, doc["version"] | "", sizeof(_new_firmware_version));

      if ((chunk_size + 256) > _MQTT_OTA_size) 
      {
        _MQTT_Client.publish((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Subscriber/MQTT_OTA_status").c_str(), "ABORTED: OVERSIZED CHUNK");
        Serial.println();
        Serial.println("[MQTT_OTA] OTA Dibatalkan: Chunk dari publisher melebihi kapasitas buffer");
        Update.abort();
        return;
      }
      
      Update.setMD5(hash_MD5);
      
      if (Update.begin(file_size))
      {
        _MQTT_Client.publish((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Subscriber/MQTT_OTA_status").c_str(), "BEGIN ACKNOWLEDGMENT: OK");
        Serial.println();
        Serial.println("[MQTT_OTA] Menerima data OTA...");
      }
      else
      {
        _MQTT_Client.publish((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Subscriber/MQTT_OTA_status").c_str(), "BEGIN ACKNOWLEDGMENT: FAILED");
        Serial.println();
        Serial.printf("[MQTT_OTA] Update.begin() gagal. Error=%d\n", Update.getError());
        Update.abort();
        return;
      }
    }
    else
    {
      _MQTT_Client.publish((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Subscriber/MQTT_OTA_status").c_str(), "ABORTED: INVALID FORMAT");
      Serial.println();
      Serial.print("[MQTT_OTA] Format payload START tidak valid (JSON Error): ");
      Serial.println(json_error.c_str());
      Update.abort();
      return;
    }
  }
  else if (strstr(topic, "/MQTT_OTA/Publisher/data") != NULL)
  {
    Update.write(payload, length);
  }
  else if (strstr(topic, "/MQTT_OTA/Publisher/end") != NULL)
  {
    Serial.println();
    Serial.println("[MQTT_OTA] Menerima sinyal END...");
    
    if (Update.end(true))
    {
      _MQTT_Client.publish((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Subscriber/MQTT_OTA_status").c_str(), "FIRMWARE VALIDATION: VALID");
      Serial.println();
      Serial.println("[MQTT_OTA] OTA Selesai dan MD5 Valid!");
      
      Serial.printf("[MQTT_OTA] Saving Firmware Version: %s\n", _new_firmware_version);
      _prefs.begin("firmware_ver", false);
      _prefs.putString("version", _new_firmware_version);
      _prefs.end();

      Serial.println("[MQTT_OTA] Restarting...");
      Serial.println();
      ESP.restart();
    }
    else
    {
      _MQTT_Client.publish((String(_MQTT_OTA_Topic)+"/MQTT_OTA/Subscriber/MQTT_OTA_status").c_str(), "FIRMWARE VALIDATION: INVALID");
      Serial.println();
      Serial.print("[MQTT_OTA] OTA Gagal (Mungkin MD5 Invalid)! Kode Error: ");
      Serial.println(Update.getError());
      Update.abort();
      return;
    }
  }
}

String MQTT_OTA::get_firmware_version() {return String(_firmware_version);}