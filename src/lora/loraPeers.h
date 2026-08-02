#ifndef _LORA_PEERS_H
#define _LORA_PEERS_H

#include <Arduino.h>

/* One entry per peer actually heard, not per possible address: a node in a
   corner of the network holds four entries, not 255. */
#ifndef LORA_MAX_PEERS
#define LORA_MAX_PEERS 16
#endif

struct LoraPeer {
  uint8_t address;
  uint32_t lastCounter;
  /* IPsec style sliding window: bit i means "counter lastCounter - 1 - i was
     already seen". Flooding delivers the same frame by several paths and out of
     order, so a plain "greater than" test would drop legitimate frames */
  uint32_t window;
  /* the counter of the last request answered, so a retry whose ACK was lost is
     acknowledged again without being executed twice */
  uint32_t acknowledgedCounter;
  uint8_t acknowledgedStatus;
  boolean hasAcknowledged;
  int16_t lastRssi;
  int8_t lastSnr;
  uint32_t lastHeardMillis;
  boolean used;
};

/* Returns the entry for address, allocating or evicting the least recently
   heard one if needed. Never returns NULL. */
LoraPeer* loraPeerGet(uint8_t address);

LoraPeer* loraPeerFind(uint8_t address);

/* Records counter for address and says whether the frame is new. A duplicate -
   the same frame arriving by another path, or a replay - answers false, which
   is both the flood suppression and the replay defence.

   A peer heard for the first time is accepted at face value: that cold entry is
   the one hole in the design, and it closes as soon as the first frame is
   recorded. */
boolean loraPeerAcceptCounter(uint8_t address, uint32_t counter);

uint8_t loraPeerCount();
LoraPeer* loraPeerAt(uint8_t index);
void loraPeerClear();

#endif
