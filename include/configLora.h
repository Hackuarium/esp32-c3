#include <Arduino.h>

// #define WIRE_SDA 3
// #define WIRE_SCL 4
// #define THR_WIRE_MASTER 1
#define THR_ONEWIRE 0  // connected on GPIO 0

extern SemaphoreHandle_t xSemaphoreWire;

// #define WIRE_SDA 6  // ESP32-C3 Seeed Studio XIAO ESP32C3
// #define WIRE_SCL 7

#define MAX_PARAM 104
extern int16_t parameters[MAX_PARAM];

#define PARAM_TEMPERATURE 0  // A
#define PARAM_HUMIDITY 1     // B
#define PARAM_PRESSURE 2     // C
#define PARAM_LUMINOSITY 3
#define PARAM_RED 4
#define PARAM_GREEN 5
#define PARAM_BLUE 6
#define PARAM_INFRA_RED 7
#define PARAM_LATITUDE 8
#define PARAM_LONGITUDE 9

#define PARAM_INT_TEMPERATURE 10
#define PARAM_INT_HUMIDITY 11

#define PARAM_WEIGHT 14
#define PARAM_WEIGHT_G 15

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_BATTERY 22    // W
#define PARAM_CHARGING 23   // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8

#define PARAM_LOGGING_INTERVAL 26         // AA - positive = s, negative = ms
#define PARAM_LOGGING_NB_ENTRIES 27       // AB
#define PARAM_LOGGING_FIRST_PARAMETER 28  // AC
#define PARAM_LOGGING_NB_PARAMETERS 29    // AD
#define PARAM_WEIGHT_OFFSET 30            // AE
#define PARAM_WEIGHT_FACTOR 31            // AF
#define PARAM_SLEEP_NORMAL_DELAY 32       // AG
#define PARAM_SLEEP_ERROR_DELAY 33        // AH

#define PARAM_WIFI_MODE 96  // CS - <=0: STA, 1: AP, 2: STA 30s then AP
