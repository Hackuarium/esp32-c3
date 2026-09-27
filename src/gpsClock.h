#ifndef _GPS_CLOCK_H
#define _GPS_CLOCK_H

#include <Arduino.h>

/* The UTC second, for a board whose radio works to a timetable.

   An NMEA sentence names the second it describes but arrives some time after
   it started - how long after is the receiver's own business, typically tens
   to a few hundred milliseconds - so a clock built on sentences alone is late
   by an amount only a measurement can tell. A PPS line, when the board has one
   wired (-D GPS_PPS=<pin>), marks the start of the second to the microsecond,
   and then the sentences only say which second it was. */
typedef struct {
  /* seconds since UTC midnight */
  uint32_t secondOfDay;
  /* millis() at which that second started: exact with a PPS, otherwise when
     the first sentence carrying it arrived */
  uint32_t startMillis;
  boolean exact;
} GpsClock;

/* False while no sentence has carried the time in the last three seconds. */
boolean gpsClockRead(GpsClock* clock);

#endif
