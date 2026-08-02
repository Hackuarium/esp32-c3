#include "config.h"
#ifdef THR_LORA_MESH
#include "lora/loraPeers.h"

#include <string.h>

static LoraPeer peers[LORA_MAX_PEERS];

LoraPeer* loraPeerFind(uint8_t address) {
  for (uint8_t i = 0; i < LORA_MAX_PEERS; i++) {
    if (peers[i].used && peers[i].address == address) {
      return &peers[i];
    }
  }
  return NULL;
}

LoraPeer* loraPeerGet(uint8_t address) {
  LoraPeer* peer = loraPeerFind(address);
  if (peer != NULL) {
    return peer;
  }

  for (uint8_t i = 0; i < LORA_MAX_PEERS; i++) {
    if (!peers[i].used) {
      peer = &peers[i];
      break;
    }
  }

  if (peer == NULL) {
    peer = &peers[0];
    for (uint8_t i = 1; i < LORA_MAX_PEERS; i++) {
      if (peers[i].lastHeardMillis < peer->lastHeardMillis) {
        peer = &peers[i];
      }
    }
  }

  memset(peer, 0, sizeof(LoraPeer));
  peer->address = address;
  peer->used = true;
  return peer;
}

boolean loraPeerAcceptCounter(uint8_t address, uint32_t counter) {
  LoraPeer* peer = loraPeerFind(address);
  if (peer == NULL) {
    peer = loraPeerGet(address);
    peer->lastCounter = counter;
    peer->lastHeardMillis = millis();
    return true;
  }

  peer->lastHeardMillis = millis();

  if (counter > peer->lastCounter) {
    uint32_t advance = counter - peer->lastCounter;
    /* a jump past the window means everything it remembered is now too old to
       matter, so it starts empty rather than shifting garbage in */
    peer->window = advance >= 32 ? 0 : ((peer->window << advance) | (1ul << (advance - 1)));
    peer->lastCounter = counter;
    return true;
  }

  uint32_t behind = peer->lastCounter - counter;
  if (behind == 0 || behind > 32) {
    return false;
  }
  uint32_t bit = 1ul << (behind - 1);
  if (peer->window & bit) {
    return false;
  }
  peer->window |= bit;
  return true;
}

uint8_t loraPeerCount() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < LORA_MAX_PEERS; i++) {
    if (peers[i].used) {
      count++;
    }
  }
  return count;
}

LoraPeer* loraPeerAt(uint8_t index) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < LORA_MAX_PEERS; i++) {
    if (peers[i].used) {
      if (count == index) {
        return &peers[i];
      }
      count++;
    }
  }
  return NULL;
}

void loraPeerClear() {
  memset(peers, 0, sizeof(peers));
}
#endif
