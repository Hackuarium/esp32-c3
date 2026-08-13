#include <Arduino.h>

/* A board that does nothing but advertise: the other end of the observer in
   src/taskBLE.cpp. No radio, no GPS, no sensors - a XIAO ESP32C3 on a cable or
   a battery, put at the far end of whatever is being measured. */
#define THR_BLE_BEACON 1

extern SemaphoreHandle_t xSemaphoreWire;

#define MAX_PARAM 26
extern int16_t parameters[MAX_PARAM];

/* dBm asked of the radio. The controller's ladder is 3 dB wide, so a value
   between two rungs is rounded down rather than up: the top of the ladder is a
   legal limit and not a preference - EN 300 328 allows 20 dBm EIRP in the
   2.4 GHz band, and the antenna is worth a couple of those. */
#define PARAM_BLE_TX_POWER 0  // A

/* Milliseconds between two advertisements. It changes nothing about how far the
   beacon reaches and everything about how long it takes to be found: an
   observer only hears the windows its own sweep happens to cover, so a beacon
   somebody is walking towards should repeat several times a second. */
#define PARAM_BLE_ADV_INTERVAL 1  // B

/* Which PHY carries it, numbered as the bridge's JSON feed reports it: 1 is the
   1M PHY and legacy PDUs, which every scanner and every phone can see; 3 is
   coded, about 7 dB further, and invisible to anything that does not run an
   extended scan. 2M is not offered - it is a secondary PHY only, and it hears
   worse than either. */
#define PARAM_BLE_PHY 2  // C

/* The loudest and most reachable thing this board can be, because that is what
   it is for. The reach is paid for in visibility: a coded beacon does not
   appear on a phone at all, which (bi) says rather than leaving it to be
   discovered. The fastest rate is part of that reach: the observer reports the
   median of its last eleven samples, so at 100 ms the reading follows somebody
   walking within about a second, where a beacon repeating once a second needs
   eleven before its median has forgotten where it was. */
#define BLE_BEACON_TX_POWER_DEFAULT 18
#define BLE_BEACON_INTERVAL_DEFAULT_MS 100
#define BLE_BEACON_PHY_DEFAULT 3

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_WIFI_MODE 23  // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8
