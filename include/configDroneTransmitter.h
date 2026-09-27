#include <Arduino.h>

/* A board that pretends to be a drone and does nothing else: the other end of
   the watcher in configDroneTracker.h. The one aircraft ever heard in the field
   was silent, so the receiver, the JSON feed and the host had nothing that
   proved them end to end. This is a known transmitter to put on the bench.

   A XIAO ESP32S3 on a cable, no LoRa, no GPS. Every second it announces a
   serial number drawn at boot, hovering over a fixed position, on three of the
   four transports the watcher listens to. NAN is the one left out: it needs a
   cluster to synchronise with, which is a Wi-Fi stack of its own. */
#define THR_DRONE_TRANSMITTER 1

extern SemaphoreHandle_t xSemaphoreWire;

#define MAX_PARAM 26
extern int16_t parameters[MAX_PARAM];

/* One switch per transport, so the watcher can be tested one path at a time.
   A and B are 1 = on, 0 = off; C is the Wi-Fi channel, 0 = off. They are
   picked up while running, since a transport that is off is the question being
   asked. A board whose qualifier is not DRONE_TX_QUALIFIER - new, or last
   running another firmware - is reset to the defaults at boot. */
#define PARAM_DRONE_TX_LEGACY 0      // A - Bluetooth 4 legacy advertising
#define PARAM_DRONE_TX_LONG_RANGE 1  // B - Bluetooth 5 Long Range, coded PHY
#define PARAM_DRONE_TX_CHANNEL 2     // C - Wi-Fi Beacon channel

/* Channel 6 is where the watcher parks by default: the social channel, on
   which an aircraft may transmit once a second. */
#define DRONE_TX_LEGACY_DEFAULT 1
#define DRONE_TX_LONG_RANGE_DEFAULT 1
#define DRONE_TX_CHANNEL_DEFAULT 6

/* Ruelle des Chataigniers 5, 1026 Denges: the building address point of
   swisstopo's register, converted from LV95 by its reframe service. The ground
   there is 397.6 m above sea level, which is 449.0 m above the WGS84 ellipsoid
   that Remote ID altitudes are measured from. Override with -D to put the
   aircraft somewhere else. */
#ifndef DRONE_TX_LATITUDE
#define DRONE_TX_LATITUDE 46.5156491
#endif
#ifndef DRONE_TX_LONGITUDE
#define DRONE_TX_LONGITUDE 6.5372752
#endif
#ifndef DRONE_TX_GROUND_ALTITUDE
#define DRONE_TX_GROUND_ALTITUDE 449.0f
#endif
/* Hovering this far above its take-off point, so it reads as airborne. The
   operator stands on the ground at the same address. */
#ifndef DRONE_TX_HEIGHT
#define DRONE_TX_HEIGHT 10.0f
#endif

#define DRONE_TX_QUALIFIER 4965

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_WIFI_MODE 23  // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8
