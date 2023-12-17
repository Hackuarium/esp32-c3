#include <Arduino.h>

extern SemaphoreHandle_t xSemaphoreWire;

#define MAX_PARAM 104
extern int16_t parameters[MAX_PARAM];

#define WIRE_SDA 6  // ESP32-C3 Seeed Studio XIAO ESP32C3
#define WIRE_SCL 7

#define LED_ON_BOARD 21

#define PARAM_ACCELERATION_X 0       // A
#define PARAM_ACCELERATION_Y 1       // B
#define PARAM_ACCELERATION_Z 2       // C
#define PARAM_ROTATION_X 3           // D
#define PARAM_ROTATION_Y 4           // E
#define PARAM_ROTATION_Z 5           // F
#define PARAM_TEMPERATURE 6          // G
#define PARAM_PRESSURE 7             // H
#define PARAM_SERVO1 8               // I
#define PARAM_SERVO2 9               // J
#define PARAM_SERVO3 10              // K
#define PARAM_OUTPUT1 11             // L
#define PARAM_OUTPUT2 12             // M
#define PARAM_OUTPUT3 13             // N
#define PARAM_UP_DOWN_COUNTER 14     // O
#define PARAM_RELATIVE_ALTITUDE 15   // P
#define PARAM_LOGGING_INTERVAL 17    // R - positive = s, negative = ms
#define PARAM_LOGGING_NB_ENTRIES 18  // S
#define PARAM_SERVO1_OFF 19          // T
#define PARAM_SERVO1_ON 20           // U
#define PARAM_ALTITUDE_PARACHUTE 21  // V
#define PARAM_ALTITUDE_GROUND 22     // W
#define PARAM_WIFI_RSSI 23           // X
#define PARAM_STATUS 24              // Y
#define PARAM_ERROR 25               // Z
#define PARAM_BATTERY1 26            // AA
#define PARAM_BATTERY2 27            // AB

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8