#include <Arduino.h>

#define LED_BUILTIN 7

#define MAX_PARAM 52
extern int16_t parameters[MAX_PARAM];

#define PARAM_TEMPERATURE 0
#define PARAM_HUMIDITY 1
#define PARAM_PRESSURE 2
#define PARAM_LUMINOSITY 3
#define PARAM_LATITUDE 4
#define PARAM_LONGITUDE 5
#define PARAM_RED 6
#define PARAM_GREEN 7
#define PARAM_BLUE 8
#define PARAM_INFRA_RED 9

#define PARAM_INT_TEMPERATURE 10
#define PARAM_INT_HUMIDITY 11
#define PARAM_INT_TEMPERATURE_A 12
#define PARAM_INT_TEMPERATURE_B 13

#define PARAM_WEIGHT 14
#define PARAM_WEIGHT_G 15

#define PARAM_BATTERY 22
#define PARAM_CHARGING 23
#define PARAM_WIFI_RSSI 24  // AX  Wifi RSSI, larger is better
#define PARAM_ERROR 25

#define PARAM_LOGGING_INTERVAL 26
#define PARAM_WEIGHT_OFFSET 27
#define PARAM_WEIGHT_FACTOR 28

#define PARAM_GATE1_IN 10
#define PARAM_GATE1_OUT 11
#define PARAM_GATE2_IN 12
#define PARAM_GATE2_OUT 13
#define PARAM_GATE3_IN 14
#define PARAM_GATE3_OUT 15
#define PARAM_GATE4_IN 16
#define PARAM_GATE4_OUT 17
#define PARAM_GATE5_IN 18
#define PARAM_GATE5_OUT 19
#define PARAM_GATE6_IN 20
#define PARAM_GATE6_OUT 21
#define PARAM_GATE7_IN 22
#define PARAM_GATE7_OUT 49
#define PARAM_GATE8_IN 50
#define PARAM_GATE8_OUT 51

// minimal 300s to prevent desctruction of EEPROM. Should last 22 years
// with 300s

#define THR_WIRE_MASTER 1