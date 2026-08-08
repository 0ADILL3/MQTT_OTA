# MQTT_OTA Library

Library MQTT OTA (Over-The-Air) dirancang khusus untuk perangkat Edge Computing di lingkungan industri. Library ini memungkinkan mikrokontroler (ESP32) untuk menerima pembaruan *firmware* secara nirkabel melalui protokol MQTT.

Menggunakan pendekatan **Dependency Injection**, library ini sangat ringan dan hemat memori karena menggunakan (*reuse*) koneksi `PubSubClient` TCP/IP tunggal yang sudah ada di program utama. 

## 🌟 Fitur Utama
- **Dependency Injection**: Tidak membuat koneksi MQTT baru.
- **MD5 Validation**: Memastikan integritas file *firmware* (tidak korup) sebelum ditulis ke memori.
- **Auto-Subscribe**: Library mengatur mekanisme *subscribe* ke topik target secara otomatis tanpa intervensi pengguna.
- **Timeout Handler**: Dilengkapi perlindungan yang membatalkan (abort) proses OTA jika aliran data terputus.

## 📦 Prasyarat (Dependencies)
Library ini menggunakan fitur bawaan inti ESP32 dan *library* pihak ketiga berikut:
- `WiFi.h`, `Update.h`, `Preferences.h` dan `ArduinoJson` (Bawaan ESP32 Core)
- `PubSubClient.h` oleh Nick O'Leary

## 📡 Alur Topik MQTT (Topic Structure)
Library ini bekerja berdasarkan prefix topik dasar yang didaftarkan. Topic dapat didaftarkan melalui `subscribe(topic)`. Jika base topic adalah `device/node_1`, maka library akan merespons sub-topik berikut:
- **`[BASE_TOPIC]/MQTT_OTA/Publisher/start`** : Menerima ukuran file, ukuran chunk, hash MD5 dan firware version dengan format:
   ```json
   {
     "file_size": 987654,
     "chunk_size": 4096,
     "hash_MD5": "e59ff97941044f85df5297e1c302d260",
     "version": "v1.0.0"
   }
   ```
- **`[BASE_TOPIC]/MQTT_OTA/Publisher/data`**  : Menerima potongan (chunk) file `.bin`.
- **`[BASE_TOPIC]/MQTT_OTA/Publisher/end`**   : Sinyal bahwa transmisi selesai dan proses flashing bisa divalidasi.
- **`[BASE_TOPIC]/MQTT_OTA/Subscriber/MQTT_OTA_status`** : Topik *feedback* dari ESP ke Broker mengenai status OTA (OK, FAILED, TIMEOUT).

## 🛠️ Cara Penggunaan (Basic Usage)
Lihat folder `examples/Basic_MQTT_OTA/Basic_MQTT_OTA.ino` untuk implementasi lengkapnya.

1. Inisialisasi Object:
   ```cpp
   #include <MQTT_OTA.h>
   PubSubClient MQTT_Client(ESP32_Client);
   MQTT_OTA MQTT_OTA_Client(MQTT_Client);
   ```
   atau dengan `MQTT_Manager`
   ```cpp
   #include <MQTT_Manager>
   #include <MQTT_OTA.h>
   MQTT_Manager MQTT_Client;
   MQTT_OTA MQTT_OTA_Client(MQTT_Client.get_client());
   ```
2. Jalankan Inisialisasi Awal: Atur target topik perangkat Anda di dalam fungsi begin(). Anda juga bisa menambahkan argumen opsional untuk timeout dan ukuran buffer jika diperlukan.
   ```cpp
   MQTT_OTA_Client.begin("device/node_1");
3. Subscribe ke Topik OTA: Library akan secara otomatis melakukan auto-subscribe melalui fungsi handle() di dalam loop.
4. Delegasikan Callback: Panggil fungsi library di dalam fungsi callback MQTT utama Anda.
   ```cpp
   void callback_MQTT(char *topic, byte *payload, unsigned int length) {
    MQTT_OTA_Client.MQTT_OTA_callback(topic, payload, length);
   }
5. jalankan fungsi `handle` pada `void loop()` untuk menangani timeout dan pendaftaran (subscribe) topik secara otomatis.
   ```cpp
   MQTT_OTA_Client.handle();

## 🐍 Script Publisher Python
Di dalam folder `tools/`, terdapat script `MQTT_OTA_firmware_publisher.py`. Script ini bertugas membaca file `firmware.bin` hasil compile, menghitung MD5, dan memecah file menjadi chunk kecil (misal 4096 bytes) untuk dikirimkan secara bertahap via MQTT (QoS=1).
Jalankan script python tersebut untuk memulai OTA

1. Buka script `MQTT_OTA_firmware_publisher.py`
2. Atur lokasi filename untuk firmware baru yang akan dipublish
3. Sesuaikan parameter seperti `MQTT_Server`, `MQTT_Topic` sesuai dengan parameter firmware lama, dan parameter `payload_max_buffer_size` serta `payload_uplad_rate` sesuai kebutuhan. (default: 8192) sesuai kemampuan dan stabilitas jaringan.
4. Jalankan script dan pantau progress nya melalui `python terminal` atau dapat juga dipantau melalui dashboard MQTT
5. Payload `FIRMWARE VALIDATION: VALID` menunjukkan proses MD5 match. payload `MQTT_OTA_DONE` menunjukkan proses upload firmware telah selesai dan ESP32 otomatis restart
6. selesai