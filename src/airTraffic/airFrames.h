#ifndef _AIR_FRAMES_H
#define _AIR_FRAMES_H

#include <stddef.h>
#include <stdint.h>

#include "airProtocols.h"

/* From what the radio captured after a sync word to a frame a host can store.

   This side does framing and integrity and nothing else: it finds which
   protocol the capture belongs to, undoes the Manchester coding and checks the
   CRC. What the bytes mean - the position, the identity, FLARM's encryption,
   ADS-L's scrambling - is left to the host, which keeps the frame as hex and
   can decode it again when its decoder improves, the same division the mesh
   makes with the body of an rx line.

   Needs neither Arduino nor a radio, so `pio test -e native` runs it. */

/* FANET's LoRa payload is the longest thing kept; the M-band decodes to 32. */
#define AIR_FRAME_MAX_BYTES 64

typedef struct {
  AirProtocol protocol;
  /* Everything after the protocol's own sync word, decoded:
     - FLARM: the 24-byte payload, then its CRC
     - ADS-L: the length byte, then that many bytes, the CRC last
     - OGN: 20 bytes, then 6 of parity
     - FANET: the LoRa payload as received */
  uint8_t bytes[AIR_FRAME_MAX_BYTES];
  uint8_t length;
  /* 1 the frame's check passed, 0 it failed, -1 nothing here checks it yet */
  int8_t check;
  /* chip pairs inside the frame that were 00 or 11, which Manchester never
     sends - a count of the bits that are guesses */
  uint8_t violations;
} AirFrame;

/* Decodes count bytes from 2 x count bytes of Manchester chips, IEEE
   convention (01 is a 1, 10 a 0). Returns the number of invalid chip pairs,
   saturating at 255; an invalid pair decodes as 0. */
uint8_t airManchesterDecode(const uint8_t* chips, uint8_t* out, size_t count);

/* CRC-16/CCITT-FALSE (polynomial 0x1021, MSB first), continued from crc. */
uint16_t airCrc16Ccitt(uint16_t crc, const uint8_t* data, size_t length);

/* The Mode S CRC-24 ADS-L uses (D.1.2), exactly as the spec writes it. */
uint32_t airCrc24(const uint8_t* data, size_t length);

/* Reads one capture made with the given setting. Returns false when it is not
   a frame of any protocol this setting hears - a sync word matched by noise -
   and the caller drops it. */
bool airFrameParse(AirListen listen, const uint8_t* raw, size_t rawLength,
                   AirFrame* frame);

#endif
