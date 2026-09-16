#ifndef _DRONE_ID_TABLE_H
#define _DRONE_ID_TABLE_H

#include <Arduino.h>
#include <opendroneid.h>

#include "droneIdQueue.h"

/* What has been heard, and from where.

   One entry is one TRANSMITTER - one address on one transport - and not one
   aircraft, which is a decision rather than an oversight. A drone broadcasting
   on both radios uses a different address on each, and the reference
   transmitter uses a different random static address again for its Bluetooth 4
   and Bluetooth 5 advertising sets, so folding them together would mean
   trusting the UAS ID: the one field a receiver is least entitled to assume is
   unique, since it is the one a spoofer picks. What is actually on the air is
   up to four transmitters, the list says so, marks the rows that claim the
   same UAS ID, and lets the operator draw the conclusion. It also keeps a
   signal strength per transport, which is the number that answers whether
   Bluetooth or Wi-Fi is reaching further today.

   Thirty-two, so that at four rows apiece it still holds eight aircraft that
   use every transport, or thirty-two that use one.

   The table is written by the drone task and read by the serial task, so
   nothing outside this file touches it: a row is copied out under the lock and
   printed afterwards, exactly as the Bluetooth observer does next door. Half a
   kilobyte is memcpy'd into a row while the console is printing the previous
   frame's position, and a reader without the lock prints a latitude from one
   frame beside a longitude from the next. */
#define DRONE_MAX_AIRCRAFT 32

typedef struct {
  boolean used;
  uint8_t address[DRONE_ADDRESS_LENGTH];
  uint8_t source;
  int8_t rssi;
  int8_t bestRssi;
  uint8_t channel;
  /* The low nibble of the last message header: 0 is ASTM F3411-19, 1 is
     ASD-STAN prEN 4709-002 P1, 2 is the current edition. Kept because the
     decoder discards it, and it says which standard an aircraft was built to -
     a version above 2 is decoded anyway, since both standards have only ever
     added fields. */
  uint8_t protocolVersion;
  uint16_t messages;
  uint32_t firstSeenMillis;
  uint32_t lastSeenMillis;
  uint32_t lastReportMillis;
  /* What the JSON feed has already said about this row - see droneIdFeed.h.
     It is paced separately from the console, and it has to be the row's rather
     than the printer's: a copy taken for printing is discarded, and pacing
     kept on a copy would pace nothing. The operator's last reported position
     is here for the same reason, since what makes the next one worth sending
     is how far it is from the one before it. */
  uint32_t lastFeedMillis;
  uint32_t lastSlowFeedMillis;
  double fedOperatorLatitude;
  double fedOperatorLongitude;
  ODID_UAS_Data record;
} DroneAircraft;

void droneIdTableBegin();
void droneIdTableReset();

/* Folds one captured frame in, and logs what it learned. A frame that does not
   decode is counted and not stored: the table is what aircraft said, not what
   the air contained. */
void droneIdTableApply(Print* output, const DroneCapture* capture);

/* Drops the transmitters that have gone quiet, saying so once each. One that
   has landed and one that has flown out of range look identical from here, so
   the line reports the last thing that was true rather than guessing which. */
void droneIdTableExpire(Print* output);

uint8_t droneIdTableCount();

/* Copies one row out under the lock. False when that slot is empty. */
boolean droneIdTableCopy(uint8_t index, DroneAircraft* destination);

/* The first earlier row claiming the same UAS ID, or -1: the same aircraft
   heard on another transport. */
int16_t droneIdTableTwin(uint8_t index);

/* Frames refused after their transport had already claimed they were Remote
   ID - a truncated pack, or a protocol squatting on the same identifier - and
   rows the table had to drop for want of space. */
uint32_t droneIdTableRejected();
uint32_t droneIdTableEvicted();

/* The last payload accepted, or the last refused, copied out so (dh) can show
   what a disagreement looked like on the air. Returns its length. */
uint8_t droneIdTableCopyLastPayload(uint8_t* destination, boolean rejected);

#endif
