#include "config.h"
#ifdef THR_DRONE_ID
#include <string.h>

#include "droneIdSurvey.h"

/* Bounded on purpose: this is a commissioning aid watching a band that is
   never quiet, and a table that grows without limit would fill with the
   building's own access points and then stop recording the one thing worth
   seeing. Twenty-four distinct identifiers is more than any room presents. */
#define SURVEY_MAX_ENTRIES 24

#define SURVEY_KIND_VENDOR 0
#define SURVEY_KIND_SERVICE 1
#define SURVEY_KIND_VENDOR_MAC 2

typedef struct {
  uint8_t kind;
  uint8_t id[3];
  uint16_t count;
} SurveyEntry;

static SurveyEntry entries[SURVEY_MAX_ENTRIES];
static uint8_t entryCount = 0;
static boolean surveying = false;
static uint32_t droneVendorFrames = 0;

/* The identifiers worth naming on sight. FA:0B:BC is ASD-STAN's and means
   Remote ID reached us and should already have been decoded; 26:37:12 is DJI's
   own, and means the aircraft is talking but in the proprietary DroneID this
   firmware does not decode - see docs/drone-rf-detection.md. */
static const uint8_t astmOui[3] = {0xFA, 0x0B, 0xBC};
static const uint8_t djiOui[3] = {0x26, 0x37, 0x12};

/* DJI's IEEE MA-L assignments, read out of the registry rather than recalled.
   A match is evidence and not proof: a MAC is trivially spoofed, and DJI
   randomises it in some modes. It is also the wrong instrument for an aircraft
   whose control link is not 802.11 at all - an OcuSync drone never emits a
   frame carrying any of these. */
static const uint8_t droneVendors[][3] = {
    {0x04, 0xA8, 0x5A}, {0x0C, 0x9A, 0xE6}, {0x20, 0x1F, 0x55},
    {0x34, 0x91, 0xF0}, {0x34, 0xD2, 0x62}, {0x48, 0x1C, 0xB9},
    {0x4C, 0x43, 0xF6}, {0x58, 0xB8, 0x58}, {0x60, 0x60, 0x1F},
    {0x88, 0x29, 0x85}, {0x8C, 0x58, 0x23}, {0x9C, 0x5A, 0x8A},
    {0xE4, 0x7A, 0x2C}, {0xEC, 0x72, 0xF7}, {0xF8, 0x40, 0x68}};
#define DRONE_VENDOR_COUNT (sizeof(droneVendors) / sizeof(droneVendors[0]))

static void record(uint8_t kind, const uint8_t* id) {
  for (uint8_t i = 0; i < entryCount; i++) {
    if (entries[i].kind == kind && memcmp(entries[i].id, id, 3) == 0) {
      if (entries[i].count < 65535) {
        entries[i].count++;
      }
      return;
    }
  }
  if (entryCount >= SURVEY_MAX_ENTRIES) {
    return;
  }
  entries[entryCount].kind = kind;
  memcpy(entries[entryCount].id, id, 3);
  entries[entryCount].count = 1;
  entryCount++;
}

static boolean isDroneVendor(const uint8_t* address) {
  for (uint8_t i = 0; i < DRONE_VENDOR_COUNT; i++) {
    if (memcmp(address, droneVendors[i], 3) == 0) {
      return true;
    }
  }
  return false;
}

void droneIdSurveyEnable(boolean enabled) { surveying = enabled; }

boolean droneIdSurveyEnabled() { return surveying; }

/* The source address is checked whether or not the survey is on: it is one
   comparison against fifteen, and an aircraft announcing itself by its
   manufacturer's OUI is worth counting on a board that is deployed rather than
   only on one being commissioned. */
void droneIdSurveyBeacon(const uint8_t* frame, uint16_t length) {
  if (length >= 16 && isDroneVendor(&frame[10])) {
    droneVendorFrames++;
    if (surveying) {
      record(SURVEY_KIND_VENDOR_MAC, &frame[10]);
    }
  }
  if (!surveying) {
    return;
  }
  /* the same walk the locator does, but recording what it finds instead of
     looking for one thing */
  uint16_t at = 36;
  while (at + 2 <= length) {
    uint8_t elementLength = frame[at + 1];
    if ((uint32_t)at + 2 + elementLength > length) {
      return;
    }
    if (frame[at] == 0xDD && elementLength >= 4) {
      record(SURVEY_KIND_VENDOR, &frame[at + 2]);
    }
    at = (uint16_t)(at + 2 + elementLength);
  }
}

void droneIdSurveyAdvertisement(const uint8_t* payload, size_t length) {
  if (!surveying) {
    return;
  }
  size_t at = 0;
  while (at + 1 < length) {
    uint8_t fieldLength = payload[at];
    if (fieldLength == 0 || at + 1 + fieldLength > length) {
      return;
    }
    /* Service Data - 16-bit UUID, which is where Remote ID would be */
    if (fieldLength >= 3 && payload[at + 1] == 0x16) {
      uint8_t id[3] = {payload[at + 2], payload[at + 3], 0};
      record(SURVEY_KIND_SERVICE, id);
    }
    at += 1 + fieldLength;
  }
}

void droneIdSurveyReset() {
  entryCount = 0;
  droneVendorFrames = 0;
}

void droneIdSurveyPrint(Print* output) {
  output->print(F("=== Survey: "));
  output->println(surveying ? F("on ===") : F("off, dv turns it on ==="));
  output->print(F("Frames from a known drone vendor OUI: "));
  output->println(droneVendorFrames);

  if (entryCount == 0) {
    output->println(F("Nothing recorded yet"));
    return;
  }
  for (uint8_t i = 0; i < entryCount; i++) {
    SurveyEntry* entry = &entries[i];
    if (entry->kind == SURVEY_KIND_SERVICE) {
      output->print(F("BLE service  "));
      /* the UUID travels least significant byte first */
      output->print(F("0x"));
      if (entry->id[1] < 0x10) {
        output->print('0');
      }
      output->print(entry->id[1], HEX);
      if (entry->id[0] < 0x10) {
        output->print('0');
      }
      output->print(entry->id[0], HEX);
    } else {
      output->print(entry->kind == SURVEY_KIND_VENDOR ? F("Wi-Fi vendor ")
                                                      : F("Wi-Fi sender "));
      for (uint8_t b = 0; b < 3; b++) {
        if (b > 0) {
          output->print(':');
        }
        if (entry->id[b] < 0x10) {
          output->print('0');
        }
        output->print(entry->id[b], HEX);
      }
    }
    output->print(F("  x"));
    output->print(entry->count);

    if (entry->kind == SURVEY_KIND_SERVICE && entry->id[1] == 0xFF &&
        entry->id[0] == 0xFA) {
      output->print(F("   <-- ASTM Remote ID, should have decoded"));
    } else if (entry->kind == SURVEY_KIND_VENDOR &&
               memcmp(entry->id, astmOui, 3) == 0) {
      output->print(F("   <-- ASTM Remote ID, should have decoded"));
    } else if (entry->kind == SURVEY_KIND_VENDOR &&
               memcmp(entry->id, djiOui, 3) == 0) {
      output->print(F("   <-- DJI DroneID, not decoded by this firmware"));
    } else if (entry->kind == SURVEY_KIND_VENDOR_MAC) {
      output->print(F("   <-- a DJI OUI"));
    }
    output->println();
  }
}
#endif
