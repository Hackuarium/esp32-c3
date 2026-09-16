#ifndef _DRONE_ID_DECODE_H
#define _DRONE_ID_DECODE_H

#include <Arduino.h>
#include <opendroneid.h>

#include "droneIdQueue.h"

/* The one call between a captured payload and the reference decoder.

   lib/opendroneid does the whole of the byte work - the nibbles, the two
   speed scales, the half-metre altitudes, the accuracy enumerations - and
   accumulates into ODID_UAS_Data across frames, which is exactly what a
   receiver needs: identity, position and the operator's position arrive as
   separate messages seconds apart and are only ever a picture once folded
   together.

   What it does not do is bounds checking. decodeOpenDroneID() takes a pointer
   and no length: it trusts that 25 bytes are readable, and for a message pack
   that the whole 3 + 25n are. On a board fed by a radio that is not a
   trustworthy assumption, so the length is checked here first. */

/* Applies one captured payload - the transport's counter byte, then a single
   message or a pack - to a record. Returns the message type applied, or
   ODID_MESSAGETYPE_INVALID when the payload was not Open Drone ID, was
   truncated, or claimed a pack larger than what arrived. */
ODID_messagetype_t droneIdDecode(const DroneCapture* capture,
                                 ODID_UAS_Data* record);

/* True once the record holds anything at all, which is what separates an
   aircraft that has been heard from a cleared slot. */
boolean droneIdDecodeHasData(const ODID_UAS_Data* record);

/* The Basic ID slot worth showing: the first one carrying an identifier, since
   an aircraft may send a serial number and a session id and the slots are
   filled in the order the messages arrive. NULL when none has been heard. */
const ODID_BasicID_data* droneIdDecodeBasicId(const ODID_UAS_Data* record);

/* Whether an ID type is text rather than 20 raw bytes. A UTM UUID printed as a
   string stops at its first zero and invents a shorter aircraft. */
boolean droneIdDecodeIdIsText(ODID_idtype_t idType);

/* Both zero is how the standard says "no position"; one zero on its own is a
   real place, and the Gulf of Guinea is not where a receiver that tested them
   separately should put an aircraft. */
boolean droneIdDecodeHasPosition(double latitude, double longitude);

#endif
