# Configuration

Configuration of the device is done using the console

# MQTT

Using the console

## Test mqtt

mosquitto_sub -h "mqtt.beemos.org" -t "test/esp"

mosquitto_pub -h 'mqtt.beemos.org' -m "123412341234 " -t "test/esp"

## Testing bidirectional communication

MQTT log publish topic: lpatiny/Beemos/hive1
MQTT publish topic: lpatiny/Beemos/hive1/a
MQTT subscribe topic: lpatiny/Beemos/hive1/q

mosquitto_sub -h "mqtt.beemos.org" -t "lpatiny/Beemos/hive1/a/+"

mosquitto_pub -h 'mqtt.beemos.org' -m "h" -t "lpatiny/Beemos/hive1/q"

## Topic

lpatiny/Beemos/hive1

## Device information

`uq` allows to setup the device information. This is a numeric number between `66*256+65` and `66*256+90` (16961 to 16986).

## Logging

- index - 4 bytes
- epoch - 4 bytes
- 26 parameters \* 2
- event - 2 bytes
- eventParameters - 2 bytes

1024 \* 1024 bytes for log
16384 logs entries

## Install platform IO cli

`export PATH=$PATH:~/.platformio/penv/bin`

```cpp
printf("Restarting now.\n");
fflush(stdout);
esp_restart();
```

`IDF_PATH=~/.platformio/packages/framework-espidf/`

You may have to add pyserial:
`sudo pip3 install pyserial`

Read device info:
`python3 $IDF_PATH/components/esptool_py/esptool/esptool.py --port /dev/cu.usbserial-A5XK3RJT flash_id`

Read partition table

`python3 $IDF_PATH/components/esptool_py/esptool/esptool.py --port /dev/cu.usbserial-A5XK3RJT read_flash 0x8000 0xc00 ptable.img`

We can now check the content of the `ptable.img` file
`python3 $IDF_PATH/components/partition_table/gen_esp32part.py ptable.img`

Here is an example of the result:

```bash
# Espressif ESP32 Partition Table
# Name, Type, SubType, Offset, Size, Flags
nvs,data,nvs,0x9000,20K,
otadata,data,ota,0xe000,8K,
app0,app,ota_0,0x10000,1280K,
app1,app,ota_1,0x150000,1280K,
spiffs,data,spiffs,0x290000,1472K,
```

## FS

Create a folder at the first level called `data`.

To upload the data

`pio run --target uploadfs`

## Alternative web server

https://github.com/me-no-dev/ESPAsyncWebServer#using-platformio
h

## OTA

pio run -t upload --upload-port square.local
pio run -t uploadfs --upload-port square.local

## Test webserver

```bash
npm i --global loadtest
loadtest -n 1000 -c 4 http://square.patiny.com/command/A
```

## Setup

Z: Layout model
AA: 0 (RGB), 1 (BRG)
AB: bit 0: day, bit 1: night
AC: Allows to turn on before or after sunset (in minutes)
AD: Allows to turn off before or after sunrise (in minutes)
AE: Minute event. <0 no action, minutes + intensity (0 to 15) _ 2000
AF: Minute event. <0 no action, minutes + intensity (0 to 15) _ 2000
AG: Minute event. <0 no action, minutes + intensity (0 to 15) _ 2000
AH: Minute event. <0 no action, minutes + intensity (0 to 15) _ 2000

AC-30: 30 minutes before sunset
AD30: 30 minutes after sunrise

AE: 60*22+2000*2 = 5320 at 10PM reduce the intensity to
AF: 60*7+2000*15 = 30420 at 7AM intensity to max

AE5320,30420,-1,-1

Full power in the evening but reduce at 10PM and turn off at 12PM
Turn on in the morning at 5AM but not too strong
AC-30,30,30960,9320,0,8300

Reduced power mode (for Christmas)
AC-30,30,8960,9320,0,8300
