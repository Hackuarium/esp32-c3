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
/* It flies a circle this wide around the centre, clockwise, this far above
   its take-off point: a kilometre at 10 m/s is one lap every 314 s. A speed of
   0 hovers over the centre instead. */
#ifndef DRONE_TX_ORBIT_DIAMETER
#define DRONE_TX_ORBIT_DIAMETER 1000.0
#endif
#ifndef DRONE_TX_SPEED
#define DRONE_TX_SPEED 10.0
#endif
#ifndef DRONE_TX_HEIGHT
#define DRONE_TX_HEIGHT 10.0f
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

#define DRONE_TX_QUALIFIER 4965

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_WIFI_MODE 23  // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8
