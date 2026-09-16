#ifndef _DRONE_ID_FRAMES_H
#define _DRONE_ID_FRAMES_H

#include <opendroneid.h>
#include <stddef.h>
#include <stdint.h>

/* Finding the Open Drone ID payload inside a frame, and nothing else.

   Three transports wrap the same messages three ways, and this is the whole of
   the difference between them. It is kept apart from the radios because it is
   the part that is easy to get wrong by one byte and impossible to notice: an
   off-by-one here decodes a valid message as garbage, or worse decodes garbage
   as a position. Nothing here touches a radio, allocates, or needs Arduino, so
   it can be exercised against frames built by the reference library's own
   transmitter on a host.

   All three return a pointer to the transport's message counter byte, with
   what follows it in payloadLength - so every caller hands the decoder the
   same shape, whichever radio it came from. NULL means this frame is not
   Remote ID. */

/* A Bluetooth advertisement, legacy or extended. The AD structures are walked
   rather than indexed at offset 6: a legacy frame has no room for anything
   ahead of the service data, but an extended one does, and the reference
   Android receiver's fixed offset misses those. */
const uint8_t* droneIdFindBluetoothPayload(const uint8_t* advertisement,
                                           size_t length,
                                           uint8_t* payloadLength);

/* An 802.11 beacon. The information elements start after the 24-byte
   management header and the 12 fixed bytes of timestamp, beacon interval and
   capability; the Remote ID one is a vendor element under the ASD-STAN
   identifier and is not required to be first or last. */
const uint8_t* droneIdFindBeaconPayload(const uint8_t* frame, uint16_t length,
                                        uint8_t* payloadLength);

/* A NAN service discovery frame. It has no elements to walk, so every field is
   at a fixed offset and the whole match is a run of comparisons. */
const uint8_t* droneIdFindNanPayload(const uint8_t* frame, uint16_t length,
                                     uint8_t* payloadLength);

/* The longest payload any of them can return: the counter byte, a pack header
   and nine 25-byte messages. */
#define DRONE_MAX_PAYLOAD (1 + 3 + ODID_MESSAGE_SIZE * ODID_PACK_MAX_MESSAGES)

/* Hands one located payload to the reference decoder, having first checked it
   is long enough - which the decoder does not do. decodeOpenDroneID() takes a
   pointer and no length: it trusts that 25 bytes are readable, and for a
   message pack it casts to a 228-byte struct and reads 3 + 25n before any of
   its own validation runs. On a board fed by a radio that is not a safe
   assumption, and the check belongs here with the rest of the framing.

   Returns the message type applied, or ODID_MESSAGETYPE_INVALID when the
   payload was too short or claimed a pack larger than what arrived. The record
   accumulates across calls, which is what a receiver needs: identity, position
   and the operator's position are separate messages seconds apart. */
ODID_messagetype_t droneIdDecodePayload(const uint8_t* payload, uint8_t length,
                                        ODID_UAS_Data* record);

#endif
