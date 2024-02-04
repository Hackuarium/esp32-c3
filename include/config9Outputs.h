#include <Arduino.h>

#define MAX_PARAM 52
extern int16_t parameters[MAX_PARAM];

#define PARAM_RED1 0    // A
#define PARAM_GREEN1 1  // B
#define PARAM_BLUE1 2   // C
#define PARAM_RED2 3    // D
#define PARAM_GREEN2 4  // E
#define PARAM_BLUE2 5   // F
#define PARAM_RED3 6    // G
#define PARAM_GREEN3 7  // H
#define PARAM_BLUE3 8   // I

#define PARAM_TIMEOUT 9  // J

#define PARAM_INPUT_1 10  // K
#define PARAM_INPUT_2 11  // L

#define PARAM_TIMER 12          // M
#define PARAM_CURRENT_TIMER 13  // N

#define PARAM_UPTIME_H 20           // U
#define PARAM_STATUS 21             // V
#define PARAM_INT_TEMPERATURE_B 22  // W
#define PARAM_WIFI_RSSI 24          // Y
#define PARAM_ERROR 25              // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0

#define PARAM_OUT1_COLOR1 26  // AA
#define PARAM_OUT1_COLOR2 27  // AB
#define PARAM_OUT1_COLOR3 28  // AC
#define PARAM_OUT1_COLOR4 29  // AD
#define PARAM_OUT1_COLOR5 30  // AE
#define PARAM_OUT1_COLOR6 31  // AF
#define PARAM_OUT1_COLOR7 32  // AG
#define PARAM_OUT1_COLOR8 33  // AH

#define PARAM_OUT2_COLOR1 34  // AI
#define PARAM_OUT2_COLOR2 35  // AJ
#define PARAM_OUT2_COLOR3 36  // AK
#define PARAM_OUT2_COLOR4 37  // AL
#define PARAM_OUT2_COLOR5 38  // AM
#define PARAM_OUT2_COLOR6 39  // AN
#define PARAM_OUT2_COLOR7 40  // AO
#define PARAM_OUT2_COLOR8 41  // AP

#define PARAM_OUT3_COLOR1 42  // AQ
#define PARAM_OUT3_COLOR2 43  // AR
#define PARAM_OUT3_COLOR3 44  // AS
#define PARAM_OUT3_COLOR4 45  // AT
#define PARAM_OUT3_COLOR5 46  // AU
#define PARAM_OUT3_COLOR6 47  // AV
#define PARAM_OUT3_COLOR7 48  // AW
#define PARAM_OUT3_COLOR8 49  // AX

#define PARAM_SPEED 50       // AY
#define PARAM_BRIGHTNESS 51  // AZ

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8