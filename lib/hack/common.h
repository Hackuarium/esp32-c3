#include <Arduino.h>

#define LED_BUILTIN 7

#define MAX_PARAM 52
extern int16_t parameters[MAX_PARAM];

#define PARAM_TEMPERATURE 0
#define PARAM_HUMIDITY 1
#define PARAM_PRESSURE 2
#define PARAM_LUMINOSITY 3
#define PARAM_RED 4
#define PARAM_GREEN 5
#define PARAM_BLUE 6
#define PARAM_INFRA_RED 7
#define PARAM_LATITUDE 8
#define PARAM_LONGITUDE 9

#define PARAM_INT_TEMPERATURE 10
#define PARAM_INT_HUMIDITY 11
#define PARAM_INT_TEMPERATURE_A 12
#define PARAM_INT_TEMPERATURE_B 13

#define PARAM_WEIGHT 14
#define PARAM_WEIGHT_G 15

#define PARAM_BATTERY 22    // W
#define PARAM_CHARGING 23   // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_LOGGING_INTERVAL 26
#define PARAM_WEIGHT_OFFSET 27
#define PARAM_WEIGHT_FACTOR 28

#define PARAM_GATE1_IN 36  // AK
#define PARAM_GATE1_OUT 37
#define PARAM_GATE2_IN 38
#define PARAM_GATE2_OUT 39
#define PARAM_GATE3_IN 40
#define PARAM_GATE3_OUT 41
#define PARAM_GATE4_IN 42
#define PARAM_GATE4_OUT 43
#define PARAM_GATE5_IN 44
#define PARAM_GATE5_OUT 45
#define PARAM_GATE6_IN 46
#define PARAM_GATE6_OUT 47
#define PARAM_GATE7_IN 48
#define PARAM_GATE7_OUT 49
#define PARAM_GATE8_IN 50
#define PARAM_GATE8_OUT 51

// minimal 300s to prevent desctruction of EEPROM. Should last 22 years
// with 300s

#define THR_WIRE_MASTER 1


/*****************************************************************************
    LOGGER
*****************************************************************************/
#define THR_LOGGER
#define NB_PARAMETERS_LINEAR_LOGS 26
#define LOG_INTERVAL 300  // Interval in (s) between logs logger

// Definition of all events to be logged
#define EVENT_ESP32_BOOT 1
#define EVENT_ESP32_SET_SAFE 2

#define EVENT_STATUS_ENABLE 3
#define EVENT_STATUS_DISABLE 4

#define EVENT_ERROR_FAILED 6
#define EVENT_ERROR_RECOVER 7

#define EVENT_ERROR_NOT_FOUND_ENTRY_N 150

#define EVENT_SAVE_ALL_PARAMETER 255
// extern const uint8_t EVENT_SAVE_ALL_PARAMETER;
// When parameters are set (and saved) an event is recorded (256-281 : A-Z +
// .... (if more parameters than 262 ...)
#define EVENT_PARAMETER_SET 256