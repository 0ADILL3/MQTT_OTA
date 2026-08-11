#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define DEBUG_MQTT_OTA 0

#if DEBUG_MQTT_OTA
  #define MQTT_OTA_LOG_F(fmt, ...) do {Serial.printf("\n[MQTT_OTA] " fmt, ##__VA_ARGS__);} while (0)
#else
  #define MQTT_OTA_LOG_F(...) do {} while (0)
#endif

#define FIRMWARE_VERSION_NAME_MAX_LEN 16
#define BASE_TOPIC_MAX_LEN            128
#define SUBSCRIBE_TOPIC_MAX_LEN       160
#define STATUS_TOPIC_MAX_LEN          160

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
    
    char topic_[BASE_TOPIC_MAX_LEN];
    uint16_t timeout_;
    uint16_t size_;
    
    bool MQTT_Client_status_ = false;
    char firmware_version_[FIRMWARE_VERSION_NAME_MAX_LEN + 1] = "v1.0.0";
    char new_firmware_version_[FIRMWARE_VERSION_NAME_MAX_LEN + 1] = {0};
    char subscribe_topic_[SUBSCRIBE_TOPIC_MAX_LEN];
    char status_topic_[STATUS_TOPIC_MAX_LEN];
    unsigned long last_time_ = 0;
  
  public:
    /**
     * @brief Konstruktor untuk inisialisasi kelas MQTT_OTA.
     * 
     * @param client Referensi ke objek PubSubClient yang sudah dikonfigurasi.
     */
    MQTT_OTA(PubSubClient &client);

    /**
     * @brief Menginisialisasi parameter pembaruan OTA dan mengambil versi firmware terakhir.
     * 
     * @param topic   Base topic untuk proses OTA (Default: "MQTT_OTA"). Topik akan otomatis disanitasi dari karakter '/' atau '#' di akhir string.
     * @param timeout Durasi maksimal (dalam milidetik) menunggu data baru sebelum OTA dibatalkan (Default: 30000 ms).
     * @param size    Ukuran buffer MQTT untuk menampung payload chunk secara utuh (Default: 8192 bytes).
     * 
     * @note Fungsi ini juga akan mengubah ukuran buffer pada instance PubSubClient secara otomatis.
     */
    void begin(const char *topic = "MQTT_OTA", uint16_t timeout = 30000, uint16_t size = 8192);

    /**
     * @brief Menangani loop utama MQTT OTA.
     * 
     * Mengecek status koneksi klien, melakukan langganan (subscribe) ulang ke topik OTA saat terhubung,
     * serta menangani pembatalan (abort) jika transmisi data melewati batas waktu (timeout).
     * 
     * @note Wajib dipanggil secara berkala di dalam fungsi loop() utama.
     */
    void handle();

    /**
     * @brief Fungsi callback (handler) untuk memproses pesan MQTT OTA yang masuk.
     * 
     * @param topic   Topik dari pesan yang diterima (BEGIN, DATA, atau END).
     * @param payload Payload dari pesan, bisa berupa JSON metadata (pada BEGIN) atau biner firmware (pada DATA).
     * @param length  Panjang byte dari payload yang diterima.
     * 
     * @note Fungsi ini akan otomatis mengeksekusi ESP.restart() jika seluruh chunk berhasil diterima dan MD5 tervalidasi.
     */
    void MQTT_OTA_callback(char *topic, byte *payload, unsigned int length);

    /**
     * @brief Mendapatkan versi firmware perangkat saat ini.
     * 
     * @return String yang merepresentasikan nama/versi firmware (misalnya: "v1.0.0").
     */
    String get_firmware_version();
};