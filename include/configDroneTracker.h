#include <Arduino.h>

/* A board that watches for drones and does nothing else.

   ASTM F3411 - the same wire format as ASD-STAN prEN 4709-002, which is what a
   European drone is built to - makes an aircraft broadcast its identity, its
   position and its operator's position in the clear, so that anybody standing
   underneath can read them. There are four ways to broadcast it and this board
   receives all four: Bluetooth 4 legacy advertising, Bluetooth 5 Long Range on
   the coded PHY, a vendor element in an ordinary Wi-Fi beacon, and a Wi-Fi NAN
   service discovery frame.

   Both radios are one radio, so they take turns - see src/taskDroneId.cpp -
   and the two window lengths are the first two parameters.

   It is a board kind of its own rather than a flag on the mesh node because
   the Wi-Fi half puts the radio in promiscuous mode and never associates:
   a station role would pin the channel to its access point's, and the two
   cannot both hold it. For the same reason the (w) menu is present but must
   not be used here - connecting to a network takes the receiver away. */
#define THR_DRONE_ID 1

/* The board is a XIAO ESP32S3 with the Wio-SX1262 on its expansion connector,
   so it is a mesh node as well: the mesh itself, its reserved block at 104 to
   113, and the MAX_PARAM of 114 that covers it all come from this one include.

   Three radios, and only two of them compete. LoRa is a separate chip on the
   SPI bus at 868 MHz, so it neither shares the 2.4 GHz front end nor the
   coexistence arbiter that Bluetooth and Wi-Fi have to take turns over.

   It is worth having for one thing above all: (ar) runs a console command on
   another node, and the (d) menu is a console command like any other. So
   `ar42:dl` lists what node 42 can see from where it is standing, which is
   what turns one drone watcher into several. */
#include "./configLoraMeshParams.h"

extern SemaphoreHandle_t xSemaphoreWire;

extern int16_t parameters[MAX_PARAM];

/* Seconds of each cycle given to each radio, 0 = that radio never listens.
   With one of them at 0 the other runs continuously; with both set, they
   alternate and each handover stops one before opening the other, so the slice
   is one the coexistence arbiter cannot take back.

   Seven and three because the aircraft decides, not us: a position goes out at
   least once a second on every transport, so three seconds of Wi-Fi is three
   chances at a beacon and five or six NAN discovery windows, and seven of
   Bluetooth is seven advertisements from each of the two Bluetooth sets. */
#define PARAM_DRONE_BLE_SECONDS 0   // A
#define PARAM_DRONE_WIFI_SECONDS 1  // B
#define DRONE_BLE_SECONDS_DEFAULT 7
#define DRONE_WIFI_SECONDS_DEFAULT 3

/* 1 to 14 pins the channel, 0 hops over 6, 1, 6, 11 - one step per Wi-Fi
   window, so channel 6 gets half of them.

   6 is the default because it is the social channel: an aircraft that uses it
   is allowed to transmit once a second and one that uses any other channel
   must transmit five times a second, so parking is what hears the slow
   transmitters and wandering is what hears the fast ones badly. NAN settles
   it - its discovery windows are 16 ms out of every 524 ms, on channel 6. */
#define PARAM_DRONE_WIFI_CHANNEL 2  // C
#define DRONE_WIFI_CHANNEL_DEFAULT 6

/* The quiet time between two lines about the same aircraft. A whole block is
   printed anyway whenever there is something new to read - a first sighting,
   or a message type that transmitter had not sent before, which is how the
   operator's position and the registration arrive minutes into a flight. This
   only paces the position lines in between: an aircraft sends one every
   second, and a console that repeats itself sixty times a minute is one nobody
   reads. 0 prints every frame. */
#define PARAM_DRONE_LOG_SECONDS 3  // D
#define DRONE_LOG_SECONDS_DEFAULT 5

/* How long an aircraft stays in the table after it goes quiet. Five minutes,
   because landing and flying out of range are the same thing from here and the
   row is what a operator looks back at to see where it went. */
#define PARAM_DRONE_FORGET_SECONDS 4  // E
#define DRONE_FORGET_SECONDS_DEFAULT 300

/* Written by the task rather than read by it: how many transmitters are in the
   table, so (s) answers "is anything flying" without (dl). */
#define PARAM_DRONE_COUNT 5  // F

/* The seconds between two (drone) lines about one transmitter on the JSON feed
   a bridge emits - see src/droneId/droneIdFeed.h. It paces a database input
   rather than a console, so it is separate from (D): what a person reading the
   port can follow and what a host wants to store are not the same rate, and on
   a board that is not a bridge this one does nothing at all.

   One second is the aircraft's own rate on the slowest transport, so nothing
   is lost by it. The feed is always paced and 0 means this default rather than
   "every frame": an aircraft transmits several times a second on each of up to
   four transports, and 0 is what an untouched slot reads. */
#define PARAM_DRONE_FEED_SECONDS 16  // Q
#define DRONE_FEED_SECONDS_DEFAULT 1

/* How far the operator must move before the feed says so again. A pilot
   standing still is the normal case and costs one line; a pilot walking a
   fence at 1.4 m/s crosses this in eighteen seconds.

   It is a threshold rather than an interval because the position it paces does
   not drift: an aircraft that reports its takeoff point reports the same
   coordinates all flight, and repeating them says nothing. */
#define PARAM_DRONE_PILOT_METRES 12  // M
#define DRONE_PILOT_METRES_DEFAULT 25

/* An identity and an operator's position are re-sent this often even when
   neither changed, because nothing else will ever repeat them: both arrive
   once, and a host that was restarting when they did would otherwise hold an
   aircraft it cannot name until it lands and flies again. */
#define DRONE_FEED_SLOW_SECONDS 300

/* What says this NVS belongs to a drone tracker rather than to whatever board
   kind was flashed here before. It has to be something no other firmware
   writes, because parameters are stored under their LETTER: a board reflashed
   from the mesh carries that firmware's slot 4 into (E), and a tell based on a
   parameter reading 0 then decides the block was configured when it was not.
   The qualifier is per board kind and already exists for exactly this. */
#define DRONE_QUALIFIER 4964

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_WIFI_MODE 23  // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8
