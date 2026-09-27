#ifndef _DRONE_ID_TRANSMIT_H
#define _DRONE_ID_TRANSMIT_H

#include <opendroneid.h>
#include <stddef.h>
#include <stdint.h>

/* The other direction of droneIdFrames: wrapping an aircraft's messages the
   way a transmitter puts them on the air. Nothing here touches a radio or
   needs Arduino, so a host test can hand every frame built here straight back
   to the receiver's locators - a transmitter and a receiver that only agree
   with each other on the bench would prove nothing.

   Only the first Basic ID is sent, and no authentication pages: this is what
   a test transmitter needs, not a complete one. */

/* One Remote ID message fills the whole of a legacy payload: length, AD type,
   UUID, application code, counter and 25 bytes. */
#define DRONE_LEGACY_ADVERTISEMENT_LENGTH 31

/* Bluetooth 4: one message of the given type. Returns 31, or 0 when the record
   holds no valid message of that type. */
size_t droneIdBuildLegacyAdvertisement(const ODID_UAS_Data* record,
                                       ODID_messagetype_t type,
                                       uint8_t counter,
                                       uint8_t* out);

/* Bluetooth 5 Long Range: the same service data holding a pack of every valid
   message. Nothing precedes it - the reference Android receiver reads the
   service data at a fixed offset and misses a pack behind Flags. Returns the
   length, or 0 when nothing fits. */
size_t droneIdBuildExtendedAdvertisement(const ODID_UAS_Data* record,
                                         uint8_t counter,
                                         uint8_t* out,
                                         size_t capacity);

/* Wi-Fi Beacon: a broadcast beacon from address, with the pack in the ASD-STAN
   vendor element. The SSID is only a label: receivers read the element. */
size_t droneIdBuildBeaconFrame(const ODID_UAS_Data* record,
                               const uint8_t address[6],
                               const char* ssid,
                               uint16_t intervalTu,
                               uint8_t counter,
                               uint8_t* out,
                               size_t capacity);

#endif
