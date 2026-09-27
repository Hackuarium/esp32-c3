#ifndef _AIR_PROTOCOLS_H
#define _AIR_PROTOCOLS_H

#include <stddef.h>
#include <stdint.h>

/* What light aircraft broadcast about themselves around 868 MHz, and how one
   SX1262 has to be set to hear each of it.

   Four systems share the band, and none of them is LoRa except FANET:

   - FLARM, the collision warning in nearly every glider, many helicopters and
     a growing share of light aircraft. 2-GFSK at 100 kchip/s, Manchester coded,
     on 868.2 and 868.4 MHz.
   - ADS-L, EASA's open standard for the same job (ADS-L 4 SRD-860). Its M-band
     is FLARM's physical layer on FLARM's two channels; its O-band is a slower
     GFSK at 869.525 MHz - the mesh's own default carrier.
   - OGN, the Open Glider Network's tracker protocol. FLARM's physical layer
     again, with a sync word of its own.
   - FANET, paragliders and hang gliders (Skytraxx, Naviter...). LoRa, SF7,
     250 kHz, on 868.2 MHz.

   A radio demodulates one modulation, on one frequency, and matches one sync
   word at a time, so a setting here is one of those combinations and the
   schedule (airSchedule.h) decides which one is on. One setting is worth more
   than the others: FLARM's sync word 0xF531FAB6 and the byte ADS-L puts in
   front of its own sync word 0x724B both begin with 0xF5, so a sync word of
   Manchester(0xF5) hears both, and what follows it says which one it was.

   Receive only. Nothing here transmits, and FLARM's own licence for its public
   protocol covers exactly that: receiving, not transmitting. */

typedef enum {
  AIR_UNKNOWN = 0,
  AIR_FLARM,
  AIR_ADSL,
  AIR_OGN,
  AIR_FANET,
} AirProtocol;

/* The bits of the protocol mask, parameter (A). One bit per listening
   setting family rather than per protocol, because FLARM and the ADS-L M-band
   cannot be told apart until after the radio has heard them. */
#define AIR_MASK_MBAND 0x01  // FLARM and ADS-L on 868.2 / 868.4
#define AIR_MASK_OGN 0x02    // OGN trackers on 868.2 / 868.4
#define AIR_MASK_FANET 0x04  // LoRa on 868.2
#define AIR_MASK_OBAND 0x08  // ADS-L on 869.525
#define AIR_MASK_ALL 0x0F

typedef enum {
  AIR_LISTEN_MBAND_LOW = 0,  // FLARM + ADS-L, 868.2 MHz
  AIR_LISTEN_MBAND_HIGH,     // FLARM + ADS-L, 868.4 MHz
  AIR_LISTEN_OGN_LOW,        // OGN, 868.2 MHz
  AIR_LISTEN_OGN_HIGH,       // OGN, 868.4 MHz
  AIR_LISTEN_FANET,          // FANET, LoRa, 868.2 MHz
  AIR_LISTEN_OBAND,          // ADS-L O-band, 869.525 MHz
  AIR_LISTEN_COUNT
} AirListen;

/* What the schedule answers when the mask leaves nothing to listen to. */
#define AIR_LISTEN_NONE AIR_LISTEN_COUNT

/* The most bytes a setting reads after its sync word: the M-band's 64 bytes of
   Manchester chips, which decode to 32. */
#define AIR_CAPTURE_MAX 64

typedef struct {
  const char* name;
  /* the bit of (A) that switches it on */
  uint8_t mask;
  bool lora;
  float frequencyMhz;

  /* GFSK only. The bit rate is the chip rate: the SX1262 has no Manchester
     decoder, so it hands over the chips and airFrames.h decodes them. */
  float bitRateKbps;
  float deviationKhz;
  float rxBandwidthKhz;
  const uint8_t* syncWord;
  uint8_t syncWordLength;
  /* bytes read after the sync word, as they came off the air */
  uint8_t captureLength;
  bool manchester;

  /* LoRa only */
  float loraBandwidthKhz;
  uint8_t spreadingFactor;
  uint8_t codingRate;
  uint8_t loraSyncWord;
} AirListenSetting;

/* The setting for one listen, or NULL for AIR_LISTEN_NONE. */
const AirListenSetting* airListenSetting(AirListen listen);

/* The name a host reads in the "proto" member of an air line. */
const char* airProtocolName(AirProtocol protocol);

#endif
