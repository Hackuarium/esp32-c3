# Configuration

Configuration of the device is done using the console

# MQTT

Using the console

## Test mqtt

mosquitto_sub -h "mqtt.hackuarium.org" -t "test/esp"

## Logging

- index - 4 bytes
- epoch - 4 bytes
- 26 parameters \* 2
- event - 2 bytes
- eventParameters - 2 bytes

1024 \* 1024 bytes for log
16384 logs entries
