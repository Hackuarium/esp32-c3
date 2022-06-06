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
