#include <Arduino.h>
#include <Preferences.h>

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