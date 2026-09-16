#include "config.h"
#ifdef THR_DRONE_ID
#include <string.h>

#include "droneIdDecode.h"
#include "droneIdFeed.h"
#include "droneIdReport.h"
#include "droneIdTable.h"
#include "params.h"

static DroneAircraft aircraft[DRONE_MAX_AIRCRAFT];
static uint32_t rejectedFrames = 0;
static uint32_t evictedRows = 0;

static uint8_t lastAccepted[DRONE_MAX_PAYLOAD];
static uint8_t lastAcceptedLength = 0;
static uint8_t lastRejected[DRONE_MAX_PAYLOAD];
static uint8_t lastRejectedLength = 0;

static SemaphoreHandle_t tableMutex = NULL;

static boolean tableTake() {
  return tableMutex != NULL &&
         xSemaphoreTake(tableMutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

static void tableGive() { xSemaphoreGive(tableMutex); }

static uint32_t secondsParameter(byte number, int16_t fallback) {
  int16_t seconds = getParameter(number);
  if (seconds == ERROR_VALUE || seconds < 0) {
    seconds = fallback;
  }
  return (uint32_t)seconds * 1000ul;
}

/* Which message types this transmitter has sent, as a bitmask, and the test
   for "there is something new to read". The Basic ID slots get a bit each
   rather than sharing one: an aircraft may send a serial number and a session
   id, they land in different slots, and folding them together would mean the
   second identity arrived with nothing said about it. */
static uint16_t sentMask(const ODID_UAS_Data* record) {
  uint16_t mask = 0;
  for (uint8_t i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
    if (record->BasicIDValid[i]) {
      mask |= 1 << i;
    }
  }
  uint8_t bit = ODID_BASIC_ID_MAX_MESSAGES;
  if (record->LocationValid) {
    mask |= 1 << bit;
  }
  if (record->SelfIDValid) {
    mask |= 1 << (bit + 1);
  }
  if (record->SystemValid) {
    mask |= 1 << (bit + 2);
  }
  if (record->OperatorIDValid) {
    mask |= 1 << (bit + 3);
  }
  for (uint8_t i = 0; i < ODID_AUTH_MAX_PAGES; i++) {
    if (record->AuthValid[i]) {
      mask |= 1 << (bit + 4);
    }
  }
  return mask;
}

static DroneAircraft* findAircraft(const uint8_t* address, uint8_t source) {
  for (uint8_t i = 0; i < DRONE_MAX_AIRCRAFT; i++) {
    if (aircraft[i].used && aircraft[i].source == source &&
        memcmp(aircraft[i].address, address, DRONE_ADDRESS_LENGTH) == 0) {
      return &aircraft[i];
    }
  }
  return NULL;
}

/* A free slot, or the transmitter heard longest ago. An eviction is counted
   rather than printed: it happens once per frame while the table is full, and
   a console that says so every second is one that has stopped being read -
   (di) is where a number belongs. */
static DroneAircraft* allocateAircraft() {
  DroneAircraft* oldest = NULL;
  for (uint8_t i = 0; i < DRONE_MAX_AIRCRAFT; i++) {
    if (!aircraft[i].used) {
      return &aircraft[i];
    }
    if (oldest == NULL ||
        (int32_t)(aircraft[i].lastSeenMillis - oldest->lastSeenMillis) < 0) {
      oldest = &aircraft[i];
    }
  }
  evictedRows++;
  return oldest;
}

void droneIdTableBegin() {
  tableMutex = xSemaphoreCreateMutex();
  memset(aircraft, 0, sizeof(aircraft));
}

void droneIdTableReset() {
  if (!tableTake()) {
    return;
  }
  memset(aircraft, 0, sizeof(aircraft));
  tableGive();
}

void droneIdTableApply(Print* output, const DroneCapture* capture) {
  /* Both are only ever touched by the drone task, so they cost stack rather
     than being on it: a row is half a kilobyte and the task also prints. */
  static ODID_UAS_Data decoded;
  static DroneAircraft snapshot;

  if (!tableTake()) {
    return;
  }
  DroneAircraft* entry = findAircraft(capture->address, capture->source);
  boolean isNew = entry == NULL;

  /* Decoded into a scratch copy first: a frame that turns out not to be Remote
     ID must not leave half of itself behind, and one malformed beacon must not
     put an aircraft in the list that never existed. */
  if (isNew) {
    odid_initUasData(&decoded);
  } else {
    memcpy(&decoded, &entry->record, sizeof(ODID_UAS_Data));
  }

  uint16_t before = sentMask(&decoded);
  const ODID_BasicID_data* previous = droneIdDecodeBasicId(&decoded);
  char previousId[ODID_ID_SIZE + 1] = "";
  if (previous != NULL) {
    memcpy(previousId, previous->UASID, sizeof(previousId));
  }

  if (droneIdDecode(capture, &decoded) == ODID_MESSAGETYPE_INVALID) {
    rejectedFrames++;
    lastRejectedLength = capture->length;
    memcpy(lastRejected, capture->payload, capture->length);
    tableGive();
    return;
  }
  lastAcceptedLength = capture->length;
  memcpy(lastAccepted, capture->payload, capture->length);

  if (isNew) {
    entry = allocateAircraft();
    memset(entry, 0, sizeof(DroneAircraft));
    entry->used = true;
    memcpy(entry->address, capture->address, DRONE_ADDRESS_LENGTH);
    entry->source = capture->source;
    entry->firstSeenMillis = millis();
    entry->bestRssi = capture->rssi;
  }

  memcpy(&entry->record, &decoded, sizeof(ODID_UAS_Data));
  entry->rssi = capture->rssi;
  if (capture->rssi > entry->bestRssi) {
    entry->bestRssi = capture->rssi;
  }
  entry->channel = capture->channel;
  entry->protocolVersion = capture->payload[1] & 0x0F;
  entry->lastSeenMillis = millis();
  if (entry->messages < 65535) {
    entry->messages++;
  }

  /* A whole block is printed when there is genuinely something new to read: a
     first sighting, or a message type this transmitter had not sent before,
     which is how the operator's position and the registration turn up minutes
     into a flight. That is bounded - the mask only ever gains bits.

     A changed UAS ID under an unchanged mask is not bounded. It is what a
     transmitter rotating a session id does, and what a spoofer does on
     purpose, so it is paced like a position line: the printing happens on the
     one task that drains the capture ring, and an unpaced block there is
     dropped frames rather than merely a noisy console. */
  const ODID_BasicID_data* current = droneIdDecodeBasicId(&decoded);
  boolean renamed =
      current != NULL && strncmp(previousId, current->UASID, ODID_ID_SIZE) != 0;
  boolean quiet =
      millis() - entry->lastReportMillis <
      secondsParameter(PARAM_DRONE_LOG_SECONDS, DRONE_LOG_SECONDS_DEFAULT);
  boolean block = isNew || sentMask(&decoded) != before || (renamed && !quiet);

  /* On a bridge the feed replaces the block: the host on the other end of the
     port parses JSON and skips everything else, so printing both would spend
     the port on lines nobody reads. The two are paced apart - (Q) against
     (D) - which is why what is due is decided here, on the row, and written
     afterwards from the copy. */
  uint8_t due = droneIdFeedDue(entry, block);
  boolean feeding = droneIdFeedEnabled();
  boolean copy = feeding ? due != 0 : (block || !quiet);

  if (copy) {
    /* Not on a bridge: lastReportMillis paces the console, and letting the
       feed's own rate write it would make (quiet) permanently true there - so
       a renamed transmitter, which is what a spoofer produces, would wait for
       the slow keepalive instead of being announced. */
    if (!feeding) {
      entry->lastReportMillis = millis();
    }
    memcpy(&snapshot, entry, sizeof(DroneAircraft));
  }
  tableGive();

  if (feeding) {
    if (copy) {
      droneIdFeedEmit(&snapshot, due);
    }
    return;
  }

  /* Printed outside the lock: a block is several hundred bytes at 115200 baud,
     and the serial task must not be held off the table for that long. */
  if (block) {
    droneIdReportAircraft(output, &snapshot, isNew);
  } else if (!quiet) {
    droneIdReportPosition(output, &snapshot);
  }
}

void droneIdTableExpire(Print* output) {
  static DroneAircraft snapshot;
  uint32_t forget = secondsParameter(PARAM_DRONE_FORGET_SECONDS,
                                     DRONE_FORGET_SECONDS_DEFAULT);
  for (uint8_t i = 0; i < DRONE_MAX_AIRCRAFT; i++) {
    if (!tableTake()) {
      return;
    }
    boolean lost =
        aircraft[i].used && millis() - aircraft[i].lastSeenMillis >= forget;
    if (lost) {
      memcpy(&snapshot, &aircraft[i], sizeof(DroneAircraft));
      aircraft[i].used = false;
    }
    tableGive();
    if (lost) {
      if (droneIdFeedEnabled()) {
        droneIdFeedLost(&snapshot);
      } else {
        droneIdReportLost(output, &snapshot);
      }
    }
  }
}

uint8_t droneIdTableCount() {
  if (!tableTake()) {
    return 0;
  }
  uint8_t count = 0;
  for (uint8_t i = 0; i < DRONE_MAX_AIRCRAFT; i++) {
    if (aircraft[i].used) {
      count++;
    }
  }
  tableGive();
  return count;
}

boolean droneIdTableCopy(uint8_t index, DroneAircraft* destination) {
  if (index >= DRONE_MAX_AIRCRAFT || !tableTake()) {
    return false;
  }
  boolean used = aircraft[index].used;
  if (used) {
    memcpy(destination, &aircraft[index], sizeof(DroneAircraft));
  }
  tableGive();
  return used;
}

int16_t droneIdTableTwin(uint8_t index) {
  if (index >= DRONE_MAX_AIRCRAFT || !tableTake()) {
    return -1;
  }
  int16_t twin = -1;
  const ODID_BasicID_data* basic =
      droneIdDecodeBasicId(&aircraft[index].record);
  if (aircraft[index].used && basic != NULL) {
    for (uint8_t i = 0; i < index; i++) {
      const ODID_BasicID_data* other =
          droneIdDecodeBasicId(&aircraft[i].record);
      if (aircraft[i].used && other != NULL &&
          strncmp(other->UASID, basic->UASID, ODID_ID_SIZE) == 0) {
        twin = (int16_t)i;
        break;
      }
    }
  }
  tableGive();
  return twin;
}

uint32_t droneIdTableRejected() { return rejectedFrames; }

uint32_t droneIdTableEvicted() { return evictedRows; }

uint8_t droneIdTableCopyLastPayload(uint8_t* destination, boolean rejected) {
  if (!tableTake()) {
    return 0;
  }
  uint8_t length = rejected ? lastRejectedLength : lastAcceptedLength;
  memcpy(destination, rejected ? lastRejected : lastAccepted, length);
  tableGive();
  return length;
}
#endif
