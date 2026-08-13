#include <Arduino.h>

/* the mesh itself, plus its reserved parameter block at 104 to 113 and the
   MAX_PARAM that covers it. This board is nothing but a mesh node, so it takes
   the whole header rather than defining any of it locally */
#include "./configLoraMeshParams.h"

#define WIRE_SDA SDA
#define WIRE_SCL SCL
#define THR_WIRE_MASTER 1

#define BMP280 0x77

#define THR_ONEWIRE 3

/* GPS_RX comes from the env: defining it turns this board into a tracker that
   broadcasts its fix on the mesh, and enables the (g) serial menu */
#ifdef GPS_RX
#define THR_GPS 1
#endif

/* BLE_SCAN comes from the env the same way: it starts the observer task and
   enables the (b) serial menu */
#ifdef BLE_SCAN
#define THR_BLE 1
#endif

extern SemaphoreHandle_t xSemaphoreWire;

#define ANALOG_SLEEP 1000
#define ANALOG_INPUTS \
  {D0, PARAM_SOLAR_MILLI_VOLTS, 10.0}, {D1, PARAM_BATTERY_MILLI_VOLTS, 2.0}

extern int16_t parameters[MAX_PARAM];

#define DHT22PIN 43  // this corresponds to D6

#define PARAM_TEMPERATURE 0  // A
#define PARAM_HUMIDITY 1     // B
#define PARAM_PRESSURE 2     // C

#define PARAM_BATTERY_MILLI_VOLTS 3  // D
#define PARAM_SOLAR_MILLI_VOLTS 4    // E
#define PARAM_SOLAR_MILLI_AMPERES 5  // F

/* GPS fix, written by taskGPS when the env defines GPS_RX. Latitude and
   longitude are degrees * 1e6, each an int32 spread over two adjacent slots.
   taskGPS only fills these eight: sending them is the generic parameter
   broadcast's job, so a tracker is "DF20 DG6 DH8", not a special frame type.

   Satellites, HDOP and fix quality travel with the fix because a position
   without them cannot be weighted: the receiver has no other way to tell a
   4-satellite 2D fix from a 12-satellite one. */
#define PARAM_GPS_LATITUDE 6      // G and H
#define PARAM_GPS_LONGITUDE 8     // I and J
#define PARAM_GPS_ALTITUDE 10     // K - meters
#define PARAM_GPS_SATELLITES 11   // L
#define PARAM_GPS_HDOP 12         // M - HDOP * 100
#define PARAM_GPS_FIX_QUALITY 13  // N - GGA field 6, 0 = no fix

#ifdef THR_BLE
/* Median RSSI in dBm of the beacon (bs) selected, ERROR_VALUE when it has not
   been heard lately. It sits at 14 and not somewhere convenient because the
   broadcast window is a run of consecutive slots, not a list: a signal
   strength that does not touch the fix cannot travel in the same frame as it,
   and a position without the reading it explains is worth little. */
#define PARAM_BLE_RSSI 14  // O
#endif

/* What (gt) puts in the broadcast window, derived rather than written twice:
   the block only works if the window covers it exactly.

   Defined only on a board that has a fix to send. A BLE listener without a GPS
   is a bridge, and a bridge reports what it hears down its own serial port -
   putting a lone RSSI on the air would spend a duty cycle to tell the host
   something it is already plugged into. */
#ifdef THR_GPS
#define PARAM_TELEMETRY_FIRST PARAM_GPS_LATITUDE
#ifdef THR_BLE
#define PARAM_TELEMETRY_LAST PARAM_BLE_RSSI
#else
#define PARAM_TELEMETRY_LAST PARAM_GPS_FIX_QUALITY
#endif
#define PARAM_TELEMETRY_BLOCK_SIZE \
  (PARAM_TELEMETRY_LAST - PARAM_TELEMETRY_FIRST + 1)
#endif

#define PARAM_RELATIVE_ALTITUDE 15  // P
#define PARAM_ALTITUDE_GROUND 16    // Q

#ifdef THR_BLE
/* What turns a dBm into a distance, and the reason it is two parameters rather
   than a constant: a VespaFinder tag publishes nothing about its own power, and
   a VFT80 does not transmit like a VFT160, so the reference is measured against
   the tag in hand with (bk). Left unset on purpose - an uncalibrated distance
   is a number that sends somebody across the wrong field.

   Deliberately after the telemetry block and not broadcast: both are constants
   of the receiver, so a host that has them once can derive the distance from
   the dBm itself, and neither is worth 2 bytes in every frame. */
#define PARAM_BLE_REFERENCE_RSSI 17  // R - dBm at 1 m, unset = uncalibrated
/* path loss exponent x 10: 20 is free space, 25 to 35 is what vegetation,
   hedges and buildings actually cost */
#define PARAM_BLE_PATH_LOSS 18  // S
#define BLE_PATH_LOSS_DEFAULT 20
/* seconds between two sweeps of the JSON feed a bridge emits, 0 = never. It is
   the whole point of a listening bridge: the operator holds a tag against it,
   reads the address off the strongest line, and sends that address to the
   tracker with (ar). */
#define PARAM_BLE_REPORT_SECONDS 19  // T
/* 5 s, because the sweep is what the operator is watching while they wave a tag
   about, and a picture that refreshes twice a minute is not something you can
   aim with. It costs nothing that is scarce: the feed goes down a serial port,
   not the radio, so a sweep of 48 devices is under a tenth of 115200 baud. The
   trade is against the reading rather than the link - a shorter window holds
   fewer advertisements, so `best` is a maximum over less evidence. */
#define BLE_REPORT_SECONDS_DEFAULT 5
#endif

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_WIFI_MODE 23  // CS - <=0: STA, 1: AP, 2: STA 30s then AP
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8

#define PARAM_LOGGING_INTERVAL 26         // AA - positive = s, negative = ms
#define PARAM_LOGGING_NB_ENTRIES 27       // AB
#define PARAM_LOGGING_FIRST_PARAMETER 28  // AC
#define PARAM_LOGGING_NB_PARAMETERS 29    // AD

#define PARAM_SLEEP_NORMAL_DELAY 32  // AG
#define PARAM_SLEEP_ERROR_DELAY 33   // AH

/* the mesh parameters themselves live at 104 to 113, see configLoraMeshParams.h */
