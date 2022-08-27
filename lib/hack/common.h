#include <Arduino.h>

#define LED_ON_BOARD 7

#define MAX_PARAM 104
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

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_BATTERY 22    // W
#define PARAM_CHARGING 23   // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8

#define PARAM_LOGGING_INTERVAL 26    // AA
#define PARAM_WEIGHT_OFFSET 27       // AB
#define PARAM_WEIGHT_FACTOR 28       // AC
#define PARAM_SLEEP_NORMAL_DELAY 29  // AD
#define PARAM_SLEEP_ERROR_DELAY 30   // AE

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

#define PARAM_BRIGHTNESS 52
#define PARAM_INTENSITY 53
#define PARAM_SPEED 54
#define PARAM_CURRENT_PROGRAM 55
#define PARAM_COLOR_MODEL 56
#define PARAM_COLOR_CHANGE_SPEED 57
#define PARAM_BACKGROUND_BRIGHTNESS 58
#define PARAM_NB_ROWS 59
#define PARAM_NB_COLUMNS 60
#define PARAM_LAYOUT_MODEL 61
#define PARAM_COLOR_LED_MODEL 62
#define PARAM_COLOR_DECREASE_SPEED 63
#define PARAM_DIRECTION 64
#define PARAM_LED_RED 65
#define PARAM_LED_GREEN 66
#define PARAM_LED_BLUE 67
#define PARAM_COMMAND_1 68
#define PARAM_COMMAND_2 69
#define PARAM_COMMAND_3 70
#define PARAM_COMMAND_4 71
#define PARAM_COMMAND_5 72
#define PARAM_COMMAND_6 73
#define PARAM_COMMAND_7 74
#define PARAM_COMMAND_8 75
#define PARAM_NB_PLAYERS 76
#define PARAM_FCT_COLOR_MODEL 77
#define PARAM_WIN_LIMIT 78
#define PARAM_SUNRISE_OFFSET 79
#define PARAM_SUNSET_OFFSET 80
// Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_1 81
#define PARAM_ACTION_2 82
#define PARAM_ACTION_3 83
#define PARAM_ACTION_4 84
#define PARAM_SCHEDULE 85

#define MAX_LED 256

#define THR_WIRE_MASTER 1