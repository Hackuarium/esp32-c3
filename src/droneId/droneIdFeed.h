#ifndef _DRONE_ID_FEED_H
#define _DRONE_ID_FEED_H

#include <Arduino.h>

#include "droneIdTable.h"

/* What a watcher on the end of a cable says, for a machine rather than for a
   person: one JSON object per line on Serial, in the feed a bridge already
   emits for the mesh.

   This is the whole path for a site whose watchers the host can reach. A post
   on a far fence has to compress what it knows into eleven bytes and spend a
   duty cycle to send it (docs/drone-mesh-forwarding.md); a board plugged into
   the machine that stores it has neither constraint, so it reports the
   position as the aircraft transmitted it, once a second, with the UAS ID on
   every line and nothing left to be bound later.

   Three events, split by how fast each one changes:

     drone   where it is now, paced by (Q)
     pilot   where the operator is standing - on arrival, and when they move
     ident   who the aircraft says it is - on arrival, and on a rename
     lost    it has stopped transmitting, or flown out of range

   The feed replaces the human block on a bridge rather than joining it: the
   same convention as loraMeshReportData, and for the same reason - a console
   block is several hundred bytes that no host will parse and every host has to
   skip. (dl) and (dd) still print for somebody reading the port.

   The line carries no address of its own, exactly as a (ble) line carries
   none: the host learns which board it is talking to once, by asking (ai) when
   it opens the port, and attributes everything on that port to it. */

#define DRONE_FEED_POSITION 0x01
#define DRONE_FEED_PILOT 0x02
#define DRONE_FEED_IDENT 0x04

/* Every caller includes config.h before this, as every .cpp here does: the
   feed is a bridge's, so a board built without the mesh has none and the
   stubs below let the call sites stay free of the question. */
#ifdef THR_LORA_MESH

/* True on a bridge, where the caller prints no human block of its own. */
boolean droneIdFeedEnabled();

/* What this sighting owes the feed, and the recording that it was sent. Called
   with the table locked, on the live row, because the pacing is the row's -
   0 when nothing is due.

   `block` is the table's own test for something new to say: a first sighting,
   a message type this transmitter had not sent before, or a changed UAS ID.
   That is what promotes an identity or an operator's position out of the
   position line, since both arrive as messages of their own, seconds or
   minutes after the first position. */
uint8_t droneIdFeedDue(DroneAircraft* entry, boolean block);

/* Writes the lines, on a copy of the row and with the lock released: a feed
   line is a couple of hundred bytes at 115200 baud, and the drone task must
   not hold the table for as long as that takes. */
void droneIdFeedEmit(const DroneAircraft* entry, uint8_t due);

/* The row has been dropped for silence. Worth a line of its own because the
   thing watching this feed has an alert open: landed and flown out of range
   are the same from here, and both of them end the track. */
void droneIdFeedLost(const DroneAircraft* entry);

#else

inline boolean droneIdFeedEnabled() { return false; }
inline uint8_t droneIdFeedDue(DroneAircraft* entry, boolean block) {
  (void)entry;
  (void)block;
  return 0;
}
inline void droneIdFeedEmit(const DroneAircraft* entry, uint8_t due) {
  (void)entry;
  (void)due;
}
inline void droneIdFeedLost(const DroneAircraft* entry) { (void)entry; }

#endif

#endif
