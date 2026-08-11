import paho.mqtt.client as MQTT
import json
from os.path import getsize
from sys import exit
from time import sleep as delay
from hashlib import md5 as MD5

filename                    = 'firmware.bin'

MQTT_Server                 = '192.168.1.100'
MQTT_Port                   = 1883
MQTT_Username               = ''
MQTT_Password               = ''
MQTT_Client_ID              = 'MQTT_OTA_firmware_publisher'
MQTT_Topic                  = 'MQTT_OTA'

firmware_version            = 'v1.0.0'

payload_chunk_size          = 4096  # 4 KB
payload_upload_rate         = 16384  # 16 KB/s

status                      = ''
timeout                     = 0
max_timeout                 = 30

SUBSCRIBER_STATUS_TOPIC     = f'{MQTT_Topic}/MQTT_OTA/Subscriber/status'
PUBLISHER_STATUS_TOPIC      = f'{MQTT_Topic}/MQTT_OTA/Publisher/status'
BEGIN_TOPIC                 = f'{MQTT_Topic}/MQTT_OTA/Publisher/begin'
DATA_TOPIC                  = f'{MQTT_Topic}/MQTT_OTA/Publisher/data'
END_TOPIC                   = f'{MQTT_Topic}/MQTT_OTA/Publisher/end'

BEGIN_ACKNOWLEDGMENT_OK     = 'BEGIN: ACKNOWLEDGMENT OK'
BEGIN_ACKNOWLEDGMENT_FAILED = 'BEGIN: ACKNOWLEDGMENT FAILED'
END_FIRMWARE_VALID          = 'END: FIRMWARE VALID'
END_FIRMWARE_INVALID        = 'END: FIRMWARE INVALID'
ABORTED_TIMEOUT             = 'ABORTED: TIMEOUT'
ABORTED_OVERSIZED_CHUNK     = 'ABORTED: OVERSIZED CHUNK'
ABORTED_INVALID_FORMAT      = 'ABORTED: INVALID FORMAT'


def calculate_MD5(file_path: str, chunk_size: int) -> str:
    hash_MD5 = MD5()
    with open(file_path, 'rb') as f:
        for chunk in iter(lambda: f.read(chunk_size), b''):
            hash_MD5.update(chunk)
    return hash_MD5.hexdigest()

def progress_bar(current_percentage: float, bar_length: int = 20) -> str:
    if current_percentage == 0:
        return f'STARTING:   [{" "*bar_length}] ...0.00 %    '
    elif current_percentage > 0 and current_percentage < 100:
        return f'PUBLISHING: [{"="*int(current_percentage/100*bar_length) + " "*(bar_length-int(current_percentage/100*bar_length))}] ...{current_percentage:.2f} %   '
    elif current_percentage == 100:
        return f'FINISH:     [{"="*bar_length}] ...100 %     '

def on_message(client, userdata, message):
    global status
    status = message.payload.decode()


MQTT_Client = MQTT.Client(MQTT_Client_ID)
MQTT_Client.username_pw_set(MQTT_Username, MQTT_Password)
MQTT_Client.connect(MQTT_Server, MQTT_Port, 120)

MQTT_Client.subscribe(SUBSCRIBER_STATUS_TOPIC)
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

payload_begin = json.dumps({
    "file_size": total_file_size,
    "chunk_size": payload_chunk_size,
    "hash_MD5": hash_MD5,
    "version": firmware_version
})
MQTT_Client.publish(BEGIN_TOPIC, payload_begin, qos=1)

while status != BEGIN_ACKNOWLEDGMENT_OK:
    if status in (BEGIN_ACKNOWLEDGMENT_FAILED, ABORTED_OVERSIZED_CHUNK, ABORTED_INVALID_FORMAT, ABORTED_TIMEOUT):
        MQTT_Client.publish(PUBLISHER_STATUS_TOPIC, status)
        print(status)
        MQTT_Client.disconnect()
        MQTT_Client.loop_stop()
        exit(1)
    
    timeout = timeout + 1
    if timeout > max_timeout:
        exit(1)
    delay(1)
timeout = 0

MQTT_Client.publish(PUBLISHER_STATUS_TOPIC, progress_bar(0))
print(progress_bar(0), end='\r')
with open(filename, 'rb') as f:
    total_chunk_buffer = 0
    while True:
        chunk = f.read(payload_chunk_size)
        if not chunk:
            break
        
        if status in (ABORTED_TIMEOUT, ):
            MQTT_Client.publish(PUBLISHER_STATUS_TOPIC, status)
            print(status)
            MQTT_Client.disconnect()
            MQTT_Client.loop_stop()
            exit(1)
        
        MQTT_Client.publish(DATA_TOPIC, chunk, qos=1)
        MQTT_Client.publish(PUBLISHER_STATUS_TOPIC, progress_bar(total_chunk_buffer/total_file_size*100))
        print(progress_bar(total_chunk_buffer/total_file_size*100), end='\r')
        total_chunk_buffer = total_chunk_buffer + len(chunk)
        
        delay(payload_chunk_size/payload_upload_rate)
MQTT_Client.publish(PUBLISHER_STATUS_TOPIC, progress_bar(100))
print(progress_bar(100), end='\r')

MQTT_Client.publish(END_TOPIC, 'END', qos=1)

while status != END_FIRMWARE_VALID:
    if status in (END_FIRMWARE_INVALID, ABORTED_TIMEOUT):
        MQTT_Client.publish(PUBLISHER_STATUS_TOPIC, status)
        print(status)
        MQTT_Client.disconnect()
        MQTT_Client.loop_stop()
        exit(1)
    
    timeout = timeout + 1
    if timeout > max_timeout:
        exit(1)
    delay(1)
timeout = 0

MQTT_Client.publish(PUBLISHER_STATUS_TOPIC, 'MQTT_OTA_DONE')
print('\nMQTT_OTA_DONE')