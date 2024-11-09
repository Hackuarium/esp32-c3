#include <Arduino.h>

#define THR_PIXELS 1
#define FRONIUS 1

#define MAX_LED 1024

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
#define PARAM_INT_TEMPERATURE_A 12
// #define PARAM_INT_TEMPERATURE_B 13

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

#define PARAM_BRIGHTNESS 52             // BA
#define PARAM_INTENSITY 53              // BB
#define PARAM_SPEED 54                  // BC
#define PARAM_CURRENT_PROGRAM 55        // BD
#define PARAM_COLOR_MODEL 56            // BE
#define PARAM_COLOR_CHANGE_SPEED 57     // BF
#define PARAM_COLOR_DECREASE_SPEED 58   // BG
#define PARAM_BACKGROUND_BRIGHTNESS 59  // BH

#define PARAM_NB_ROWS 60     // BI
#define PARAM_NB_COLUMNS 61  // BJ

#define PARAM_LED_RED 62    // BK
#define PARAM_LED_GREEN 63  // BL
#define PARAM_LED_BLUE 64   // BM

#define PARAM_FCT_COLOR_MODEL 65  // BN
#define PARAM_DIRECTION 66        // BO
#define PARAM_COMMAND_1 67        // BP
#define PARAM_COMMAND_2 68        // BQ
#define PARAM_COMMAND_3 69        // BR
#define PARAM_COMMAND_4 70        // BS
#define PARAM_NB_PLAYERS 71       // BT
#define PARAM_WIN_LIMIT 72        // BU
#define PARAM_COMMAND_5 73        // BV
#define PARAM_COMMAND_6 74        // BW
#define PARAM_COMMAND_7 75        // BX
#define PARAM_COMMAND_8 76        // BY

#define PARAM_LAYOUT_MODEL 77     // BZ
#define PARAM_COLOR_LED_MODEL 78  // CA  NEO_RGB: 6 - NEO_GRB: 82
#define PARAM_SCHEDULE 79         // CB - 1 day, 2 night, 3 day+night
// Allows to turn on before or after sunset (in minutes)
#define PARAM_SUNSET_OFFSET 80   // CC
#define PARAM_SUNRISE_OFFSET 81  // CD
// Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_1 82  // CE
// Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_2 83  // CF
// Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_3 84  // CG
// Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_4 85  // CH
#define PARAM_ACTION_5 86  // CI
#define PARAM_ACTION_6 87  // CJ
#define PARAM_ACTION_7 88  // CK
#define PARAM_ACTION_8 89  // CL

#define PARAM_METEO_SLOT_1 90  // CM
#define PARAM_METEO_SLOT_2 91  // CN
#define PARAM_METEO_SLOT_3 92  // CO
#define PARAM_METEO_SLOT_4 93  // CP
#define PARAM_METEO_SLOT_5 94  // CQ
#define PARAM_METEO_SLOT_6 95  // CR

#define SLOT_METEO_TEMPERATURE 0     // temperature / rain
#define SLOT_METEO_WEATHER_ICON 1    // weather icon
#define SLOT_METEO_FRONIUS 2         // fronius
#define SLOT_METEO_SUNRISE_SUNSET 3  // sunrise / sunset