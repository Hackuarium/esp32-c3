#include "airSchedule.h"

/* The settings that can fill a window of the gap, by what the mask allows.
   With neither gap protocol enabled the window is not wasted: ADS-L on the
   M-band is not slotted, so it can arrive then. */
static AirListen gapListen(uint8_t mask, uint32_t cycle) {
  bool fanet = mask & AIR_MASK_FANET;
  bool oband = mask & AIR_MASK_OBAND;
  if (fanet && oband) {
    return cycle % AIR_SHARE_TURN == AIR_SHARE_TURN - 1 ? AIR_LISTEN_OBAND
                                                        : AIR_LISTEN_FANET;
  }
  if (fanet) {
    return AIR_LISTEN_FANET;
  }
  if (oband) {
    return AIR_LISTEN_OBAND;
  }
  if (mask & AIR_MASK_MBAND) {
    return AIR_LISTEN_MBAND_LOW;
  }
  if (mask & AIR_MASK_OGN) {
    return AIR_LISTEN_OGN_LOW;
  }
  return AIR_LISTEN_NONE;
}

AirListen airListenAtUtc(uint8_t mask, uint32_t secondOfDay,
                         uint16_t millisInSecond) {
  bool mband = mask & AIR_MASK_MBAND;
  bool ogn = mask & AIR_MASK_OGN;

  /* the first 200 ms of a second still belong to the previous one's slot 1 */
  uint32_t cycle =
      millisInSecond < AIR_GAP_START_MS ? secondOfDay - 1 : secondOfDay;

  if (millisInSecond >= AIR_GAP_START_MS &&
      millisInSecond < AIR_SLOT0_START_MS) {
    return gapListen(mask, cycle);
  }
  /* nothing slotted to follow: the gap settings keep the radio all second */
  if (!mband && !ogn) {
    return gapListen(mask, cycle);
  }

  bool followOgn =
      !mband || (ogn && cycle % AIR_SHARE_TURN == AIR_SHARE_TURN - 1);
  bool slot0 = millisInSecond >= AIR_SLOT0_START_MS &&
               millisInSecond < AIR_SLOT1_START_MS;
  if (followOgn) {
    return slot0 ? AIR_LISTEN_OGN_HIGH : AIR_LISTEN_OGN_LOW;
  }
  return slot0 ? AIR_LISTEN_MBAND_LOW : AIR_LISTEN_MBAND_HIGH;
}

AirListen airListenRotating(uint8_t mask, uint32_t turn) {
  uint8_t enabled = 0;
  for (uint8_t i = 0; i < AIR_LISTEN_COUNT; i++) {
    if (mask & airListenSetting((AirListen)i)->mask) {
      enabled++;
    }
  }
  if (enabled == 0) {
    return AIR_LISTEN_NONE;
  }
  uint8_t wanted = turn % enabled;
  for (uint8_t i = 0; i < AIR_LISTEN_COUNT; i++) {
    if (mask & airListenSetting((AirListen)i)->mask) {
      if (wanted == 0) {
        return (AirListen)i;
      }
      wanted--;
    }
  }
  return AIR_LISTEN_NONE;
}
