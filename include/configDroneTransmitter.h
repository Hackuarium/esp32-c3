#include <Arduino.h>

/* A board that pretends to be a drone and does nothing else: the other end of
   the watcher in configDroneTracker.h. The one aircraft ever heard in the field
   was silent, so the receiver, the JSON feed and the host had nothing that
   proved them end to end. This is a known transmitter to put on the bench.

   A XIAO ESP32S3 on a cable, no LoRa, no GPS. Every second it announces a
   serial number drawn at boot, flying a circle around a fixed position, on
   three of the four transports the watcher listens to. NAN is the one left out: it needs a
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

/* The centre both circles are flown around. The ground is taken as 449 m
   above the WGS84 ellipsoid that Remote ID altitudes are measured from.
   Override with -D to put the aircraft somewhere else. */
#ifndef DRONE_TX_LATITUDE
#define DRONE_TX_LATITUDE 46.5172371
#endif
#ifndef DRONE_TX_LONGITUDE
#define DRONE_TX_LONGITUDE 6.5395828
#endif
#ifndef DRONE_TX_GROUND_ALTITUDE
#define DRONE_TX_GROUND_ALTITUDE 449.0f
#endif
/* It flies a circle this wide around the centre, clockwise: a kilometre at
   10 m/s is one lap every 314 s. A speed of 0 hovers over the centre instead. */
#ifndef DRONE_TX_ORBIT_DIAMETER
#define DRONE_TX_ORBIT_DIAMETER 1000.0
#endif
#ifndef DRONE_TX_SPEED
#define DRONE_TX_SPEED 10.0
#endif
/* Its height above take-off rises and falls between these two, as a sine of
   this period, so a 3D view has a climb to show. 120 s against the 314 s lap
   makes the track a spiral rather than one loop flown over and over, and peaks
   at 2.6 m/s vertically - a multirotor's, not a rocket's. Equal heights fly
   level. */
#ifndef DRONE_TX_HEIGHT_MIN
#define DRONE_TX_HEIGHT_MIN 50.0
#endif
#ifndef DRONE_TX_HEIGHT_MAX
#define DRONE_TX_HEIGHT_MAX 150.0
#endif
#ifndef DRONE_TX_HEIGHT_PERIOD
#define DRONE_TX_HEIGHT_PERIOD 120.0
#endif

/* The operator walks a circle of their own around the same centre, at walking
   pace, and is reported as live GNSS - what a pilot holding a phone sends. At
   1.4 m/s they pass the watchers' 25 m move threshold every 18 s or so, so the
   operator's track moves too. A speed of 0 keeps them standing at the centre,
   reported as a fixed location. */
#ifndef DRONE_TX_OPERATOR_DIAMETER
#define DRONE_TX_OPERATOR_DIAMETER 100.0
#endif
#ifndef DRONE_TX_OPERATOR_SPEED
#define DRONE_TX_OPERATOR_SPEED 1.4
#endif

/* Wi-Fi transmit power, in the driver's 0.25 dBm steps. The XIAO ESP32S3
   radiates nothing at the 20 dBm the driver starts with: esp_wifi_80211_tx
   still returns ESP_OK for every beacon, so the counters climb while the air
   stays empty. Measured on the bench with a laptop scanning for the soft AP:
   never heard at 20 dBm, heard only some of the time at 19.5, always at 15
   and below - the board-level fault reported in espressif/arduino-esp32#8770.
   Bluetooth, at +9 dBm, was never affected, which is why the watchers heard
   BT 4 and BT 5 from the start. */
#ifndef DRONE_TX_WIFI_POWER
#define DRONE_TX_WIFI_POWER 60
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
