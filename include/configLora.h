#include <Arduino.h>

#define WIRE_SDA SDA
#define WIRE_SCL SCL
#define THR_WIRE_MASTER 1

#define BMP280 0x77

#define THR_ONEWIRE 3
#define THR_LORA_SEND 1

extern SemaphoreHandle_t xSemaphoreWire;

#define ANALOG_SLEEP 1000
#define ANALOG_INPUTS \
  {D0, PARAM_SOLAR_MILLI_VOLTS, 10.0}, {D1, PARAM_BATTERY_MILLI_VOLTS, 2.0}

#define MAX_PARAM 36
extern int16_t parameters[MAX_PARAM];

#define DHT22PIN 43  // this corresponds to D6

#define PARAM_TEMPERATURE 0  // A
#define PARAM_HUMIDITY 1     // B
#define PARAM_PRESSURE 2     // C

#define PARAM_BATTERY_MILLI_VOLTS 3  // D
#define PARAM_SOLAR_MILLI_VOLTS 4    // E
#define PARAM_SOLAR_MILLI_AMPERES 5  // F

#define PARAM_RELATIVE_ALTITUDE 15  // P
#define PARAM_ALTITUDE_GROUND 16    // Q

#define PARAM_LORA_SLEEP_SECONDS 17     // R
#define PARAM_LORA_INTERVAL_SECONDS 18  // S

#define PARAM_LORA_WAN_VERSION 19  // T - 0: 1.0, 1: 1.1
#define PARAM_UPTIME_H 20          // U
#define PARAM_STATUS 21            // V
#define PARAM_WIFI_MODE 23         // CS - <=0: STA, 1: AP, 2: STA 30s then AP
#define PARAM_WIFI_RSSI 24         // Y
#define PARAM_ERROR 25             // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8

#define PARAM_LOGGING_INTERVAL 26         // AA - positive = s, negative = ms
#define PARAM_LOGGING_NB_ENTRIES 27       // AB
#define PARAM_LOGGING_FIRST_PARAMETER 28  // AC
#define PARAM_LOGGING_NB_PARAMETERS 29    // AD
#define PARAM_SLEEP_NORMAL_DELAY 32       // AG
#define PARAM_SLEEP_ERROR_DELAY 33        // AH
