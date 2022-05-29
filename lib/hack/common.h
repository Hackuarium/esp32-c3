#include <Arduino.h>

#define LED_BUILTIN 7

#define MAX_PARAM 52
extern int16_t parameters[MAX_PARAM];

#define MAX_LED 1024

#define PARAM_ERROR 51

#define PARAM_BRIGHTNESS 0  // A
#define PARAM_INTENSITY 1
#define PARAM_SPEED 2
#define PARAM_CURRENT_PROGRAM 3        // D
#define PARAM_COLOR_MODEL 4            // E
#define PARAM_COLOR_CHANGE_SPEED 5     // 0 to 6 - F
#define PARAM_COLOR_DECREASE_SPEED 6   // 0 to 6 - G
#define PARAM_BACKGROUND_BRIGHTNESS 7  // 0 or 1 - H

#define PARAM_NB_ROWS 8     // I
#define PARAM_NB_COLUMNS 9  // J

#define PARAM_RED 10    // K: 0 to 255
#define PARAM_GREEN 11  // L: 0 to 255
#define PARAM_BLUE 12   // M: 0 to 255

#define PARAM_FCT_COLOR_MODEL 13  // N
#define PARAM_DIRECTION 14        // O
#define PARAM_COMMAND_1 15        // P
#define PARAM_COMMAND_2 16        // Q
#define PARAM_COMMAND_3 17        // R
#define PARAM_COMMAND_4 18        // S
#define PARAM_NB_PLAYERS 19       // T
#define PARAM_WIN_LIMIT 20        // U
#define PARAM_COMMAND_5 21        // V
#define PARAM_COMMAND_6 22        // W
#define PARAM_COMMAND_7 23        // X
#define PARAM_COMMAND_8 24        // Y

#define PARAM_LAYOUT_MODEL 25     // Z
#define PARAM_COLOR_LED_MODEL 26  // AA
#define PARAM_SCHEDULE 27         // AB   bit 0: day, bit 1: night
#define PARAM_SUNSET_OFFSET \
  28  // AC   Allows to turn on before or after sunset (in minutes)
#define PARAM_SUNRISE_OFFSET \
  29  // AD   Allows to turn off before or after sunrise (in minutes)
#define PARAM_ACTION_1 \
  30  // AE   Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_2 \
  31  // AF   Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_3 \
  32  // AG   Minute event. <0 no action, minutes + intensity (0 to 15) * 2000
#define PARAM_ACTION_4 \
  33  // AH   Minute event. <0 no action, minutes + intensity (0 to 15) * 2000

#define PARAM_TEMPERATURE 34
#define PARAM_HUMIDITY 35

#define PARAM_WIFI_RSSI 49  // AX  Wifi RSSI, larger is better