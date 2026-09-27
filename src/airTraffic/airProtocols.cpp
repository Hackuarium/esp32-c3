#include "airProtocols.h"

/* Manchester here is the IEEE convention every one of these protocols uses: a
   1 is sent as the chips 01 and a 0 as 10, so the byte 0xF5 goes out as
   0x55 0x99 (ADS-L 4 SRD-860, C.2.1).

   0xF5 is common ground. ADS-L's preamble must end on the chips 1001 1001
   (C.2.3), which is the Manchester of the nibble 5, and SoftRF, which
   interoperates with both, describes FLARM's sync word as 0xF531FAB6 and
   ADS-L's as 0xF5724B18 - the second being the spec's sync word 0x724B followed
   by the packet length 24. So the radio matches the 16 chips of 0xF5 and the
   task reads what follows: 31 FA B6 is FLARM, 72 4B is ADS-L.

   Sixteen chips is a short sync word, so noise matches it far more often than
   it would an eight-byte one. A false start costs one capture, 5 ms, and
   airFrames.h drops it because what follows is neither protocol; (ti) counts
   them, so the price is measured rather than assumed. The alternative is
   hearing only one of the two. */
static const uint8_t mbandSyncWord[] = {0x55, 0x99};

/* Manchester(0x0AF3656C), the sync word of the OGN tracker protocol - as SoftRF
   and the OGN trackers themselves send it, after one byte of 0xAA preamble.
   Eight bytes, so it never matches by accident. */
static const uint8_t ognSyncWord[] = {0xAA, 0x66, 0x55, 0xA5,
                                      0x96, 0x99, 0x96, 0x5A};

/* ADS-L's O-band sync word is 0x2DD4 after ten bytes of 0xAA (C.3.1, C.3.2).
   Two bytes of the preamble go in front of it, as SoftRF does, so that two
   bytes of sync word cannot match early inside the preamble. */
static const uint8_t obandSyncWord[] = {0xAA, 0xAA, 0x2D, 0xD4};

/* The M-band is 100 kchip/s with a +-50 kHz deviation and a Gaussian BT of
   0.5, in a 200 kHz channel (C.2). 234.3 kHz is the narrowest receive filter
   the SX1262 offers that holds it with room for two crystals disagreeing.

   What is read after the sync word:
   - M-band: FLARM's 31 FA B6, 24 bytes, a 2-byte CRC - 29 bytes, 58 chips -
     or ADS-L's 72 4B, the length 24 and 24 bytes - 27, 54 chips. 64 covers
     both.
   - OGN: 20 bytes and 6 of parity, 52 chips.
   - O-band: not Manchester coded. The length, then 24 bytes for the one
     payload ADS-L defines so far; 32 leaves room.

   FANET is SF7 at 250 kHz, coding rate 4/5, and the sync word the SX127x calls
   0xF1 (SoftRF's FANET+). RadioLib converts it for the SX1262. */
static const AirListenSetting settings[AIR_LISTEN_COUNT] = {
    {"M-band 868.2", AIR_MASK_MBAND, false, 868.2f, 100.0f, 50.0f, 234.3f,
     mbandSyncWord, sizeof(mbandSyncWord), 64, true, 0, 0, 0, 0},
    {"M-band 868.4", AIR_MASK_MBAND, false, 868.4f, 100.0f, 50.0f, 234.3f,
     mbandSyncWord, sizeof(mbandSyncWord), 64, true, 0, 0, 0, 0},
    {"OGN 868.2", AIR_MASK_OGN, false, 868.2f, 100.0f, 50.0f, 234.3f,
     ognSyncWord, sizeof(ognSyncWord), 52, true, 0, 0, 0, 0},
    {"OGN 868.4", AIR_MASK_OGN, false, 868.4f, 100.0f, 50.0f, 234.3f,
     ognSyncWord, sizeof(ognSyncWord), 52, true, 0, 0, 0, 0},
    {"FANET 868.2", AIR_MASK_FANET, true, 868.2f, 0, 0, 0, NULL, 0, 0, false,
     250.0f, 7, 5, 0xF1},
    /* 38.4 kbit/s, +-10 kHz (C.3). The filter is wider than the signal needs
       because at 869 MHz two 20 ppm crystals can already be 35 kHz apart. */
    {"O-band 869.525", AIR_MASK_OBAND, false, 869.525f, 38.4f, 10.0f, 117.3f,
     obandSyncWord, sizeof(obandSyncWord), 32, false, 0, 0, 0, 0},
};

const AirListenSetting* airListenSetting(AirListen listen) {
  if (listen >= AIR_LISTEN_COUNT) {
    return NULL;
  }
  return &settings[listen];
}

const char* airProtocolName(AirProtocol protocol) {
  switch (protocol) {
    case AIR_FLARM:
      return "flarm";
    case AIR_ADSL:
      return "adsl";
    case AIR_OGN:
      return "ogn";
    case AIR_FANET:
      return "fanet";
    default:
      return "unknown";
  }
}
