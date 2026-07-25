import paho.mqtt.client as MQTT
from os.path import getsize
from sys import exit
from time import sleep as delay
from hashlib import md5

filename            = '../../../.pio/build/esp32s3/firmware.bin'

MQTT_Server         = '192.168.8.119'
MQTT_Port           = 1883
MQTT_Username       = ''
MQTT_Password       = ''
MQTT_Client_ID      = 'Python_MQTT_OTA_firmware_publisher'
MQTT_Topic          = 'rsmd/environment/cold_storage/realtime'

firmware_version    = 'v3.0'

payload_chunk_size  = 4096  # 4 KB
payload_upload_rate = 8192  # 8 KB/s

MQTT_OTA_status     = ''
timeout             = 0


def calculate_MD5(file_path, chunk_size):
    hash_MD5 = md5()
    with open(file_path, 'rb') as f:
        for chunk in iter(lambda: f.read(chunk_size), b''):
            hash_MD5.update(chunk)
    return hash_MD5.hexdigest()

def on_message(client, userdata, message):
    global MQTT_OTA_status
    MQTT_OTA_status = message.payload.decode()


MQTT_Client = MQTT.Client(MQTT_Client_ID)
MQTT_Client.username_pw_set(MQTT_Username, MQTT_Password)
MQTT_Client.connect(MQTT_Server, MQTT_Port, 120)

MQTT_Client.subscribe(f'{MQTT_Topic}/MQTT_OTA/Subscriber/MQTT_OTA_status')
MQTT_Client.on_message = on_message
MQTT_Client.loop_start()


total_file_size = getsize(filename)
hash_MD5 = calculate_MD5(filename, payload_chunk_size)

print(f'Memulai proses update firmware via MQTT_OTA...')
print(f'Ukuran file           : {total_file_size} bytes')
print(f'Ukuran chunk          : {payload_chunk_size} bytes')
print(f'MD5 dikirim           : {hash_MD5}')
print(f'Firmware version      : {firmware_version}')
print(f'Interval chunk upload : {payload_chunk_size/payload_upload_rate:.2f} detik')
print(f'Kecepatan upload      : {payload_upload_rate} bytes/detik')
print(f'Jumlah chunk          : {int(total_file_size/payload_chunk_size)+1} chunk')
print(f'Total waktu upload    : {total_file_size/payload_upload_rate:.2f} detik')

payload_start = f'<{total_file_size}><{payload_chunk_size}><{hash_MD5}><{firmware_version}>'
MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/start', payload_start, qos=1)

while MQTT_OTA_status != 'BEGIN ACKNOWLEDGMENT: OK':
    if MQTT_OTA_status in ('BEGIN ACKNOWLEDGMENT: FAILED', 'ABORTED: OVERSIZED CHUNK', 'ABORTED: INVALID FORMAT', 'ABORTED: TIMEOUT'):
        MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/MQTT_OTA_status', MQTT_OTA_status)
        print(MQTT_OTA_status)
        MQTT_Client.disconnect()
        MQTT_Client.loop_stop()
        exit(1)
    
    timeout = timeout + 1
    if timeout > 10:
        exit(1)
    delay(1)
timeout = 0

MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/MQTT_OTA_status', f'STARTING:   [{" "*20}] ...0.00 %')
print(f'STARTING:   [{" "*20}] ...0.00 %', end='\r')
with open(filename, 'rb') as f:
    total_buffer = 0
    while True:
        chunk = f.read(payload_chunk_size)
        if not chunk:
            break
        
        if MQTT_OTA_status in ('ABORTED: TIMEOUT', ):
            MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/MQTT_OTA_status', MQTT_OTA_status)
            print(MQTT_OTA_status)
            MQTT_Client.disconnect()
            MQTT_Client.loop_stop()
            exit(1)
        
        MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/data', chunk, qos=1)
        MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/MQTT_OTA_status', f'PUBLISHING: [{"="*int(total_buffer/total_file_size*20) + " "*(20-int(total_buffer/total_file_size*20))}] ...{total_buffer/total_file_size*100:.2f} %')
        print(f'PUBLISHING: [{"="*int(total_buffer/total_file_size*20) + " "*(20-int(total_buffer/total_file_size*20))}] ...{total_buffer/total_file_size*100:.2f} %', end='\r')
        total_buffer = total_buffer + len(chunk)
        
        delay(payload_chunk_size/payload_upload_rate)
MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/MQTT_OTA_status', f'FINISH:     [{"="*20}] ...100 %')
print(f'FINISH:     [{"="*20}] ...100 %', end='\r')

MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/end', 'END', qos=1)

while MQTT_OTA_status != 'FIRMWARE VALIDATION: VALID':
    if MQTT_OTA_status in ('FIRMWARE VALIDATION: INVALID', 'ABORTED: TIMEOUT'):
        MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/MQTT_OTA_status', MQTT_OTA_status)
        print(MQTT_OTA_status)
        MQTT_Client.disconnect()
        MQTT_Client.loop_stop()
        exit(1)
    
    timeout = timeout + 1
    if timeout > 10:
        exit(1)
    delay(1)
timeout = 0

MQTT_Client.publish(f'{MQTT_Topic}/MQTT_OTA/Publisher/MQTT_OTA_status', 'MQTT_OTA_DONE')
print('\nMQTT_OTA_DONE')