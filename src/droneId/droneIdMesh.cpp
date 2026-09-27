#include "config.h"
#if defined(THR_DRONE_ID) && defined(THR_LORA_MESH)
#include <string.h>

#include "../lora/loraBridge.h"
#include "../lora/loraMesh.h"
#include "droneIdDecode.h"
#include "droneIdFeed.h"
#include "droneIdMesh.h"
#include "droneIdRecords.h"
#include "droneIdReport.h"
#include "droneIdTable.h"
#include "params.h"

/* Sixteen aircraft at once over one fence is already a swarm; an aircraft that
   finds no handle is counted rather than evicting one, since re-binding a live
   handle to another UAS ID is the one thing the host cannot detect. */
#define DRONE_MESH_MAX_HANDLES 16

/* The age nibble tops out at 15, and past it a position is not worth the
   airtime. Longer than (K) on purpose: with Bluetooth and Wi-Fi taking turns an
   aircraft on one transport goes unheard for seven seconds at a time. */
#define DRONE_MESH_STALE_MILLIS 15000ul

/* Two transmitters claiming one UAS ID, heard within two seconds of each other
   and further apart than this: each claims under 30 m of accuracy, and 20 m/s
   for two seconds is another 40. */
#define DRONE_MESH_CONFLICT_METRES 100
#define DRONE_MESH_CONFLICT_WINDOW_MS 2000

/* A burst of new aircraft must not turn one tick into a second of transmitting
   with the receiver closed; the rest go on the next tick. */
#define DRONE_MESH_MAX_IDENTS_PER_TICK 2

/* An IDENT is one unacknowledged broadcast, sent back to back with the PILOT
   and TRACK of the same tick, so one lost to a collision left the handle
   unnamed on the host until the five minute keepalive. A new handle's IDENT is
   therefore sent on its first three ticks: two more frames per new aircraft. */
#define DRONE_MESH_IDENT_REPEATS 3

typedef struct {
  boolean used;
  boolean present;
  uint8_t handle;
  char uasId[ODID_ID_SIZE];
  uint8_t idType;
  uint8_t uaType;
  boolean tracked;

  /* gathered afresh on every pass */
  uint32_t heardMillis;
  uint8_t transports;
  boolean conflict;
  boolean hasLocation;
  boolean located;
  uint32_t locationMillis;
  int8_t rssi;
  ODID_Location_data location;
  boolean hasPilot;
  uint32_t pilotSeenMillis;
  double pilotLatitude;
  double pilotLongitude;
  uint8_t pilotSource;
  uint8_t classification;
  boolean hasOperatorId;
  char operatorId[ODID_ID_SIZE];

  /* what has already been said */
  uint8_t identsSent;
  boolean identHadOperator;
  boolean identHadClass;
  uint32_t identMillis;
  boolean pilotSent;
  double sentPilotLatitude;
  double sentPilotLongitude;
  uint32_t pilotMillis;
} MeshHandle;

static MeshHandle handles[DRONE_MESH_MAX_HANDLES];
static uint8_t nextHandle = 0;
static uint32_t lastTickMillis = 0;
static volatile int16_t requestedIdent = -1;
static uint32_t framesSent = 0;
static uint32_t framesRefused = 0;
static uint32_t aircraftUnreported = 0;

static int16_t intervalSeconds() {
  int16_t seconds = getParameter(PARAM_DRONE_MESH_SECONDS);
  return seconds == ERROR_VALUE || seconds < 0 ? 0 : seconds;
}

static uint8_t perFrame() {
  int16_t count = getParameter(PARAM_DRONE_MESH_PER_FRAME);
  return count >= 1 && count <= 4 ? (uint8_t)count
                                  : DRONE_MESH_PER_FRAME_DEFAULT;
}

static boolean isNewer(uint32_t millisA, uint32_t millisB) {
  return (int32_t)(millisA - millisB) > 0;
}

static MeshHandle* findHandle(const char* uasId) {
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    if (handles[i].used &&
        memcmp(handles[i].uasId, uasId, ODID_ID_SIZE) == 0) {
      return &handles[i];
    }
  }
  return NULL;
}

static boolean handleInUse(uint8_t handle) {
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    if (handles[i].used && handles[i].handle == handle) {
      return true;
    }
  }
  return false;
}

static MeshHandle* allocateHandle(const ODID_BasicID_data* basic) {
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    if (!handles[i].used) {
      MeshHandle* entry = &handles[i];
      memset(entry, 0, sizeof(MeshHandle));
      entry->used = true;
      while (handleInUse(nextHandle)) {
        nextHandle++;
      }
      entry->handle = nextHandle++;
      memcpy(entry->uasId, basic->UASID, ODID_ID_SIZE);
      return entry;
    }
  }
  aircraftUnreported++;
  return NULL;
}

static void gatherLocation(MeshHandle* entry, const DroneAircraft* row) {
  const ODID_Location_data* location = &row->record.Location;
  boolean located =
      droneIdDecodeHasPosition(location->Latitude, location->Longitude);
  if (located && entry->located &&
      abs((int32_t)(row->locationMillis - entry->locationMillis)) <=
          DRONE_MESH_CONFLICT_WINDOW_MS &&
      droneIdMetresBetween(entry->location.Latitude, entry->location.Longitude,
                           location->Latitude, location->Longitude) >
          DRONE_MESH_CONFLICT_METRES) {
    entry->conflict = true;
  }
  /* A row with a position beats one without, then the newest Location wins -
     not the row heard last, which over Bluetooth 4 may have sent nothing but
     its serial number for minutes. */
  boolean better = !entry->hasLocation || (located && !entry->located) ||
                   (located == entry->located &&
                    isNewer(row->locationMillis, entry->locationMillis));
  if (better) {
    entry->hasLocation = true;
    entry->located = located;
    entry->locationMillis = row->locationMillis;
    memcpy(&entry->location, location, sizeof(ODID_Location_data));
  }
}

static void gatherRow(MeshHandle* entry, const DroneAircraft* row) {
  if (!entry->present || isNewer(row->lastSeenMillis, entry->heardMillis)) {
    entry->heardMillis = row->lastSeenMillis;
  }
  entry->present = true;
  entry->transports |= (uint8_t)(1 << row->source);
  if (millis() - row->lastSeenMillis < DRONE_MESH_STALE_MILLIS &&
      row->rssi > entry->rssi) {
    entry->rssi = row->rssi;
  }
  if (row->record.LocationValid) {
    gatherLocation(entry, row);
  }
  const ODID_System_data* system = &row->record.System;
  if (row->record.SystemValid) {
    if (droneIdDecodeHasPosition(system->OperatorLatitude,
                                 system->OperatorLongitude) &&
        (!entry->hasPilot ||
         isNewer(row->lastSeenMillis, entry->pilotSeenMillis))) {
      entry->hasPilot = true;
      entry->pilotSeenMillis = row->lastSeenMillis;
      entry->pilotLatitude = system->OperatorLatitude;
      entry->pilotLongitude = system->OperatorLongitude;
      entry->pilotSource = system->OperatorLocationType;
    }
    if (system->ClassificationType == ODID_CLASSIFICATION_TYPE_EU) {
      entry->classification =
          (uint8_t)((system->CategoryEU << 4) | (system->ClassEU & 0x0F));
    }
  }
  if (row->record.OperatorIDValid) {
    entry->hasOperatorId = true;
    memcpy(entry->operatorId, row->record.OperatorID.OperatorId, ODID_ID_SIZE);
  }
}

/* Folds the transmitter table into one entry per UAS ID. A handle no row claims
   any more is released: its transmitters were forgotten after (E) seconds of
   silence, and the host infers the loss from the TRACKs stopping. */
static void gather() {
  static DroneAircraft row;
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    MeshHandle* entry = &handles[i];
    entry->present = false;
    entry->transports = 0;
    entry->conflict = false;
    entry->hasLocation = false;
    entry->located = false;
    entry->rssi = -128;
    entry->hasPilot = false;
    entry->classification = 0;
    entry->hasOperatorId = false;
  }
  for (uint8_t i = 0; i < DRONE_MAX_AIRCRAFT; i++) {
    if (!droneIdTableCopy(i, &row)) {
      continue;
    }
    const ODID_BasicID_data* basic = droneIdDecodeBasicId(&row.record);
    if (basic == NULL) {
      continue;
    }
    MeshHandle* entry = findHandle(basic->UASID);
    if (entry == NULL) {
      entry = allocateHandle(basic);
      if (entry == NULL) {
        continue;
      }
    }
    entry->idType = basic->IDType;
    entry->uaType = basic->UAType;
    gatherRow(entry, &row);
  }
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    if (handles[i].used && !handles[i].present) {
      handles[i].used = false;
    }
  }
}

static boolean send(const uint8_t* body, size_t length) {
  if (length == 0) {
    return false;
  }
  boolean sent = loraMeshSend(LORA_ADDRESS_BROADCAST, LORA_TYPE_DATA, body,
                              (uint8_t)length, loraMeshSilent());
  if (sent) {
    framesSent++;
  } else {
    framesRefused++;
  }
  return sent;
}

static uint32_t slowMillis() {
  return (uint32_t)DRONE_FEED_SLOW_SECONDS * 1000ul;
}

static void sendIdents() {
  uint8_t body[LORA_MAX_BODY_SIZE];
  uint8_t sent = 0;
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    MeshHandle* entry = &handles[i];
    if (!entry->present || sent >= DRONE_MESH_MAX_IDENTS_PER_TICK) {
      continue;
    }
    boolean asked = requestedIdent == entry->handle;
    boolean due = entry->identsSent < DRONE_MESH_IDENT_REPEATS || asked ||
                  (entry->hasOperatorId && !entry->identHadOperator) ||
                  (entry->classification != 0 && !entry->identHadClass) ||
                  millis() - entry->identMillis >= slowMillis();
    if (!due) {
      continue;
    }
    DroneIdent ident = {entry->handle,         entry->idType,
                        entry->uaType,         entry->classification,
                        entry->uasId,
                        entry->hasOperatorId ? entry->operatorId : NULL};
    sent++;
    if (send(body, droneIdEncodeIdent(&ident, body, sizeof(body)))) {
      if (entry->identsSent < DRONE_MESH_IDENT_REPEATS) {
        entry->identsSent++;
      }
      entry->identHadOperator = entry->hasOperatorId;
      entry->identHadClass = entry->classification != 0;
      entry->identMillis = millis();
      if (asked) {
        requestedIdent = -1;
      }
    }
  }
}

static void sendPilots() {
  const uint8_t maximum = (LORA_MAX_BODY_SIZE - 2) / DRONE_PILOT_RECORD_SIZE;
  DronePilot pilots[maximum];
  MeshHandle* owners[maximum];
  uint8_t count = 0;
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES && count < maximum; i++) {
    MeshHandle* entry = &handles[i];
    if (!entry->present || !entry->hasPilot) {
      continue;
    }
    boolean moved = entry->pilotSent &&
                    droneIdMetresBetween(entry->sentPilotLatitude,
                                         entry->sentPilotLongitude,
                                         entry->pilotLatitude,
                                         entry->pilotLongitude) >=
                        droneIdPilotMoveMetres();
    if (entry->pilotSent && !moved &&
        millis() - entry->pilotMillis < slowMillis()) {
      continue;
    }
    pilots[count] = {entry->handle, entry->pilotSource, moved, false,
                     entry->pilotLatitude, entry->pilotLongitude};
    owners[count++] = entry;
  }
  uint8_t body[LORA_MAX_BODY_SIZE];
  if (count == 0 ||
      !send(body, droneIdEncodePilot(pilots, count, body, sizeof(body)))) {
    return;
  }
  for (uint8_t i = 0; i < count; i++) {
    owners[i]->pilotSent = true;
    owners[i]->sentPilotLatitude = owners[i]->pilotLatitude;
    owners[i]->sentPilotLongitude = owners[i]->pilotLongitude;
    owners[i]->pilotMillis = millis();
  }
}

static void flushTracks(DroneTrack* tracks, MeshHandle** owners, uint8_t count) {
  uint8_t body[LORA_MAX_BODY_SIZE];
  if (count == 0 ||
      !send(body, droneIdEncodeTrack(tracks, count, body, sizeof(body)))) {
    return;
  }
  for (uint8_t i = 0; i < count; i++) {
    owners[i]->tracked = true;
  }
}

/* An aircraft with no recent Location - none sent yet, one still solving its
   fix, or one heard only through its other messages lately - is still
   reported, with the position bit clear: the
   TRACK is what says it is still being heard, and at what margin, and without
   it the host holds an IDENT every five minutes and nothing in between. */
static void fillTrack(DroneTrack* track,
                      const MeshHandle* entry,
                      boolean fresh) {
  memset(track, 0, sizeof(DroneTrack));
  track->handle = entry->handle;
  track->conflict = entry->conflict;
  track->first = !entry->tracked;
  track->rssi = entry->rssi;
  track->transports = entry->transports;
  if (!fresh) {
    track->height = INV_ALT;
    track->speed = INV_SPEED_H;
    track->heading = INV_DIR;
    return;
  }
  const ODID_Location_data* location = &entry->location;
  track->status = location->Status;
  track->heightAboveTakeoff =
      location->HeightType == ODID_HEIGHT_REF_OVER_TAKEOFF;
  track->latitude = entry->located ? location->Latitude : 0;
  track->longitude = entry->located ? location->Longitude : 0;
  track->height = location->Height;
  track->speed = location->SpeedHorizontal;
  track->heading = location->Direction;
}

static void sendTracks() {
  DroneTrack tracks[4];
  MeshHandle* owners[4];
  uint8_t count = 0;
  uint8_t limit = perFrame();
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    MeshHandle* entry = &handles[i];
    /* a position older than the stale window is not sent as if it were
       current: the aircraft is still reported, without one */
    boolean fresh =
        entry->hasLocation &&
        millis() - entry->locationMillis < DRONE_MESH_STALE_MILLIS;
    uint32_t silent =
        millis() - (fresh ? entry->locationMillis : entry->heardMillis);
    if (!entry->present || silent >= DRONE_MESH_STALE_MILLIS) {
      continue;
    }
    fillTrack(&tracks[count], entry, fresh);
    tracks[count].secondsSinceHeard = silent / 1000;
    owners[count++] = entry;
    if (count == limit) {
      flushTracks(tracks, owners, count);
      count = 0;
    }
  }
  flushTracks(tracks, owners, count);
}

void droneIdMeshService() {
  if (loraMeshIsBridge()) {
    return;
  }
  int16_t interval = intervalSeconds();
  boolean tick = interval > 0 &&
                 millis() - lastTickMillis >= (uint32_t)interval * 1000ul;
  if (!tick && requestedIdent < 0) {
    return;
  }
  gather();
  sendIdents();
  if (tick) {
    lastTickMillis = millis();
    sendPilots();
    sendTracks();
  }
}

static void printHandles(Print* output) {
  uint8_t live = 0;
  for (uint8_t i = 0; i < DRONE_MESH_MAX_HANDLES; i++) {
    const MeshHandle* entry = &handles[i];
    if (!entry->used) {
      continue;
    }
    live++;
    output->print(F("  handle "));
    output->print(entry->handle);
    output->print(F("  "));
    if (droneIdDecodeIdIsText((ODID_idtype_t)entry->idType)) {
      output->write((const uint8_t*)entry->uasId,
                    strnlen(entry->uasId, ODID_ID_SIZE));
    } else {
      output->print(F("(binary id)"));
    }
    for (uint8_t source = 0; source < 4; source++) {
      if ((entry->transports & (1 << source)) != 0) {
        output->print(F(" "));
        output->print(droneIdSourceLabel(source));
      }
    }
    output->println(entry->identsSent > 0 ? F("") : F(", IDENT not sent yet"));
  }
  if (live == 0) {
    output->println(F("  nothing being reported"));
  }
}

static void printCost(Print* output, int16_t interval) {
  uint8_t count = perFrame();
  uint16_t percent = loraMeshBodyBudgetPercent(
      (uint8_t)(2 + count * DRONE_TRACK_RECORD_SIZE), interval);
  output->print(F("Airtime with "));
  output->print(count);
  output->print(F(" aircraft flying: "));
  output->print(percent);
  output->println(F("% of the duty cycle"));
  if (percent > 100) {
    output->print(F("Over budget - "));
    output->print(100 - 10000 / percent);
    output->println(F("% of TRACK frames will be dropped"));
    output->print(F("Either dm"));
    output->print(((uint32_t)interval * percent + 99) / 100);
    output->println(F(", or a faster spreading factor such as DE7"));
  }
}

void droneIdMeshCommand(const char* paramValue, Print* output) {
  if (paramValue[0] != '\0') {
    int value = atoi(paramValue);
    if (value < 0 || value > 3600) {
      output->println(F("Seconds between reports, 0 to 3600"));
      return;
    }
    setAndSaveParameter(PARAM_DRONE_MESH_SECONDS, (int16_t)value);
  }
  int16_t interval = intervalSeconds();
  if (loraMeshIsBridge()) {
    output->println(F("Mesh forwarding: not on a bridge - its own port "
                      "carries the feed"));
  } else if (interval == 0) {
    output->println(F("Mesh forwarding: off (dm5 reports every 5 s)"));
  } else {
    output->print(F("Mesh forwarding: every "));
    output->print(interval);
    output->print(F(" s, "));
    output->print(perFrame());
    output->println(F(" aircraft per frame"));
    printCost(output, interval);
  }
  if (!loraMeshHasKey()) {
    output->println(F("No AES128 key set, nothing can be sent (ak)"));
  }
  if (!loraMeshTake(output)) {
    return;
  }
  printHandles(output);
  output->print(F("Frames sent: "));
  output->print(framesSent);
  output->print(F(", refused: "));
  output->println(framesRefused);
  if (aircraftUnreported > 0) {
    output->print(F("Aircraft left out, no free handle: "));
    output->println(aircraftUnreported);
  }
  loraMeshGive();
}

void droneIdMeshRequestIdent(const char* paramValue, Print* output) {
  if (paramValue[0] == '\0') {
    output->println(F("Which handle? df3 re-sends the IDENT of handle 3"));
    return;
  }
  int handle = atoi(paramValue);
  if (handle < 0 || handle > 255 || !loraMeshTake(output)) {
    return;
  }
  boolean known = handleInUse((uint8_t)handle);
  loraMeshGive();
  if (!known) {
    output->println(F("No aircraft has that handle"));
    return;
  }
  requestedIdent = (int16_t)handle;
  output->println(F("IDENT queued"));
}

#endif
