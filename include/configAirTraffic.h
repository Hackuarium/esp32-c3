#include <Arduino.h>

/* A board that listens to what light aircraft broadcast around 868 MHz:
   FLARM, ADS-L, the OGN tracker protocol and FANET - see
   src/airTraffic/airProtocols.h for the four and docs/air-traffic.md for why
   one radio cannot hear them all at once.

   A XIAO ESP32S3 with the Wio-SX1262, the same hardware as a mesh node, but
   not a mesh node: the SX1262 is the receiver here, retuned several times a
   second, and a radio doing that cannot also hold the mesh's channel. So it
   reports down its USB port, one JSON line per frame heard, to a host that is
   holding the other end.

   Receive only. It transmits nothing, which is also the whole of what FLARM's
   licence for its public protocol permits. */
#define THR_AIR_TRAFFIC 1

/* The GPS is what turns a guess into a timetable: FLARM and OGN transmit in
   slots counted from the UTC second, and knowing the second is what lets one
   radio be on the right channel for each slot (src/airTraffic/airSchedule.h).
   Without a receiver the settings take turns and hear a share of everything.
   A PPS line on -D GPS_PPS=<pin> makes the second exact; without one it is
   dated by the sentences, corrected by (C). */
#ifdef GPS_RX
#define THR_GPS 1
#define PARAM_GPS_LATITUDE 6      // G and H
#define PARAM_GPS_LONGITUDE 8     // I and J
#define PARAM_GPS_ALTITUDE 10     // K - meters
#define PARAM_GPS_SATELLITES 11   // L
#define PARAM_GPS_HDOP 12         // M - HDOP * 100
#define PARAM_GPS_FIX_QUALITY 13  // N - GGA field 6, 0 = no fix
#endif

extern SemaphoreHandle_t xSemaphoreWire;

#define MAX_PARAM 26
extern int16_t parameters[MAX_PARAM];

/* Which of the four to listen to, as bits: 1 FLARM and ADS-L on 868.2/868.4
   (one setting hears both), 2 OGN, 4 FANET, 8 ADS-L on 869.525. The O-band is
   off by default because almost nothing transmits there yet, and every
   setting switched on takes time from the others. */
#define PARAM_AIR_PROTOCOLS 0  // A
#define AIR_PROTOCOLS_DEFAULT 7

/* Milliseconds per turn when there is no clock and the settings simply take
   turns. A second, because an aircraft transmits about once a second: a turn
   shorter than that misses whoever has not spoken yet. */
#define PARAM_AIR_TURN_MS 1  // B
#define AIR_TURN_MS_DEFAULT 1000

/* How late the GPS sentences arrive after the second they name, in
   milliseconds, so the timetable can start the second earlier by as much.
   Unknown until measured: every frame is reported with the millisecond it
   landed at, and FLARM frames should cluster between 400 and 1200 - the shift
   that puts them there is this value. Ignored when a PPS line is wired. */
#define PARAM_AIR_CLOCK_OFFSET_MS 2  // C
#define AIR_CLOCK_OFFSET_MS_DEFAULT 0

/* Written by the task: frames reported in the last full minute, so (s) says
   whether anything is flying without (ti). */
#define PARAM_AIR_FRAMES 5  // F

/* What says this NVS belongs to an air traffic receiver rather than to the
   board kind flashed here before - see configDroneTracker.h. */
#define AIR_QUALIFIER 4966

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_WIFI_MODE 23  // X
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8
