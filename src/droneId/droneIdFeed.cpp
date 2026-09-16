#include "config.h"
#if defined(THR_DRONE_ID) && defined(THR_LORA_MESH)
#include <math.h>
#include <string.h>

#include "../lora/loraBridge.h"
#include "droneIdDecode.h"
#include "droneIdFeed.h"
#include "droneIdReport.h"
#include "params.h"

/* The feed is always paced. An aircraft transmits its position several times a
   second on each of up to four transports, and what is on the other end of the
   port is a database rather than a log - so a value of 0, which is what an
   untouched slot reads, means the default and not "every frame". */
static uint32_t feedIntervalMillis() {
  int16_t seconds = getParameter(PARAM_DRONE_FEED_SECONDS);
  if (seconds == ERROR_VALUE || seconds <= 0) {
    seconds = DRONE_FEED_SECONDS_DEFAULT;
  }
  return (uint32_t)seconds * 1000ul;
}

static int16_t pilotMoveMetres() {
  int16_t metres = getParameter(PARAM_DRONE_PILOT_METRES);
  if (metres == ERROR_VALUE || metres <= 0) {
    metres = DRONE_PILOT_METRES_DEFAULT;
  }
  return metres;
}

/* Equirectangular, which is exact enough for a threshold of a few tens of
   metres and costs one cosine: the two points are a walk apart, never a
   flight. */
static double metresBetween(double fromLatitude,
                            double fromLongitude,
                            double toLatitude,
                            double toLongitude) {
  const double metresPerDegree = 111320.0;
  double latitude = (toLatitude - fromLatitude) * metresPerDegree;
  double longitude = (toLongitude - fromLongitude) * metresPerDegree *
                     cos(fromLatitude * M_PI / 180.0);
  return sqrt(latitude * latitude + longitude * longitude);
}

boolean droneIdFeedEnabled() { return loraMeshIsBridge(); }

uint8_t droneIdFeedDue(DroneAircraft* entry, boolean block) {
  if (!droneIdFeedEnabled()) {
    return 0;
  }
  uint8_t due = 0;

  if (block || millis() - entry->lastFeedMillis >= feedIntervalMillis()) {
    entry->lastFeedMillis = millis();
    due |= DRONE_FEED_POSITION;
  }

  /* Both of these are re-sent on a slow keepalive as well as on arrival. A
     line can be lost to a host that was restarting, and neither of them will
     be repeated by anything else: an operator standing still and a serial
     number are precisely the things that never change again. */
  boolean slow = entry->lastSlowFeedMillis == 0 ||
                 millis() - entry->lastSlowFeedMillis >=
                     (uint32_t)DRONE_FEED_SLOW_SECONDS * 1000ul;

  if (droneIdDecodeBasicId(&entry->record) != NULL && (block || slow)) {
    due |= DRONE_FEED_IDENT;
  }

  const ODID_System_data* system = &entry->record.System;
  if (entry->record.SystemValid &&
      droneIdDecodeHasPosition(system->OperatorLatitude,
                               system->OperatorLongitude)) {
    boolean moved =
        !droneIdDecodeHasPosition(entry->fedOperatorLatitude,
                                  entry->fedOperatorLongitude) ||
        metresBetween(entry->fedOperatorLatitude, entry->fedOperatorLongitude,
                      system->OperatorLatitude, system->OperatorLongitude) >=
            pilotMoveMetres();
    if (moved || block || slow) {
      entry->fedOperatorLatitude = system->OperatorLatitude;
      entry->fedOperatorLongitude = system->OperatorLongitude;
      due |= DRONE_FEED_PILOT;
    }
  }

  if (slow && (due & (DRONE_FEED_IDENT | DRONE_FEED_PILOT)) != 0) {
    entry->lastSlowFeedMillis = millis();
  }
  return due;
}

static void feedAddress(Print* json, const DroneAircraft* entry) {
  char address[DRONE_ADDRESS_LENGTH * 3];
  uint8_t at = 0;
  for (uint8_t i = 0; i < DRONE_ADDRESS_LENGTH; i++) {
    if (i > 0) {
      address[at++] = ':';
    }
    at += (uint8_t)sprintf(address + at, "%02x", entry->address[i]);
  }
  loraBridgeTextBytes(json, "addr", address, at);
}

/* The identifier as the aircraft gave it: text when the ID type is text, and
   the 20 bytes in hex when it is not. A UTM UUID printed as a string stops at
   its first zero and invents a shorter aircraft. */
static void feedUasId(Print* json, const ODID_BasicID_data* basic) {
  if (droneIdDecodeIdIsText(basic->IDType)) {
    loraBridgeTextBytes(json, "uas", basic->UASID,
                        (uint8_t)strnlen(basic->UASID, ODID_ID_SIZE));
    return;
  }
  char hex[ODID_ID_SIZE * 2 + 1];
  for (uint8_t i = 0; i < ODID_ID_SIZE; i++) {
    sprintf(hex + i * 2, "%02x", (uint8_t)basic->UASID[i]);
  }
  loraBridgeTextBytes(json, "uasHex", hex, ODID_ID_SIZE * 2);
}

/* Every line carries the transmitter it came from and the identity claimed on
   it, so that one line is a complete statement and the host never has to hold
   two of them together to read either. */
static Print* feedBegin(const char* event, const DroneAircraft* entry) {
  Print* json = loraBridgeBegin(event);
  if (json == NULL) {
    return NULL;
  }
  feedAddress(json, entry);
  loraBridgeText(json, "via", droneIdSourceLabel(entry->source));
  const ODID_BasicID_data* basic = droneIdDecodeBasicId(&entry->record);
  if (basic != NULL) {
    feedUasId(json, basic);
  }
  return json;
}

static void feedPosition(const DroneAircraft* entry) {
  const ODID_Location_data* location = &entry->record.Location;
  Print* json = feedBegin("drone", entry);
  if (json == NULL) {
    return;
  }
  loraBridgeInt(json, "status", location->Status);
  if (droneIdDecodeHasPosition(location->Latitude, location->Longitude)) {
    loraBridgeFloat(json, "lat", location->Latitude, 7);
    loraBridgeFloat(json, "lon", location->Longitude, 7);
  }
  /* The standard's own sentinel for every one of these is -1000 m, and a
     missing key is the only honest way to carry it: a height of -1000 stored
     as a number is an aircraft a kilometre underground. */
  if (location->AltitudeGeo > INV_ALT) {
    loraBridgeFloat(json, "alt", location->AltitudeGeo, 1);
  }
  if (location->Height > INV_ALT) {
    loraBridgeFloat(json, "height", location->Height, 1);
    loraBridgeInt(json, "heightRef", location->HeightType);
  }
  if (location->SpeedHorizontal < INV_SPEED_H) {
    loraBridgeFloat(json, "speed", location->SpeedHorizontal, 2);
  }
  if (location->SpeedVertical < INV_SPEED_V) {
    loraBridgeFloat(json, "vspeed", location->SpeedVertical, 2);
  }
  if (location->Direction < INV_DIR) {
    loraBridgeFloat(json, "heading", location->Direction, 1);
  }
  loraBridgeInt(json, "hacc", location->HorizAccuracy);
  loraBridgeInt(json, "vacc", location->VertAccuracy);
  loraBridgeInt(json, "rssi", entry->rssi);
  if (entry->channel > 0) {
    loraBridgeInt(json, "ch", entry->channel);
  }
  loraBridgeEnd(json);
}

static void feedPilot(const DroneAircraft* entry) {
  const ODID_System_data* system = &entry->record.System;
  Print* json = feedBegin("pilot", entry);
  if (json == NULL) {
    return;
  }
  loraBridgeFloat(json, "lat", system->OperatorLatitude, 7);
  loraBridgeFloat(json, "lon", system->OperatorLongitude, 7);
  if (system->OperatorAltitudeGeo > INV_ALT) {
    loraBridgeFloat(json, "alt", system->OperatorAltitudeGeo, 1);
  }
  /* Where the coordinates came from, which is what says whether this is a
     person or a memory: a takeoff position is where the aircraft left the
     ground and stays there all flight, a live one follows the controller. */
  loraBridgeInt(json, "source", system->OperatorLocationType);
  if (system->ClassificationType == ODID_CLASSIFICATION_TYPE_EU) {
    loraBridgeInt(json, "category", system->CategoryEU);
    loraBridgeInt(json, "class", system->ClassEU);
  }
  loraBridgeInt(json, "rssi", entry->rssi);
  loraBridgeEnd(json);
}

static void feedIdent(const DroneAircraft* entry) {
  const ODID_BasicID_data* basic = droneIdDecodeBasicId(&entry->record);
  Print* json = feedBegin("ident", entry);
  if (json == NULL) {
    return;
  }
  loraBridgeInt(json, "idType", basic->IDType);
  loraBridgeInt(json, "uaType", basic->UAType);
  if (entry->record.OperatorIDValid) {
    const ODID_OperatorID_data* operatorId = &entry->record.OperatorID;
    loraBridgeTextBytes(
        json, "operator", operatorId->OperatorId,
        (uint8_t)strnlen(operatorId->OperatorId, ODID_ID_SIZE));
  }
  if (entry->record.SelfIDValid) {
    loraBridgeTextBytes(
        json, "selfId", entry->record.SelfID.Desc,
        (uint8_t)strnlen(entry->record.SelfID.Desc, ODID_STR_SIZE));
  }
  loraBridgeInt(json, "version", entry->protocolVersion);
  loraBridgeEnd(json);
}

void droneIdFeedLost(const DroneAircraft* entry) {
  Print* json = feedBegin("lost", entry);
  if (json == NULL) {
    return;
  }
  loraBridgeInt(json, "seen", (int32_t)((millis() - entry->firstSeenMillis) / 1000));
  loraBridgeInt(json, "silent", (int32_t)((millis() - entry->lastSeenMillis) / 1000));
  loraBridgeInt(json, "messages", entry->messages);
  loraBridgeInt(json, "best", entry->bestRssi);
  loraBridgeEnd(json);
}

void droneIdFeedEmit(const DroneAircraft* entry, uint8_t due) {
  if ((due & DRONE_FEED_IDENT) != 0) {
    feedIdent(entry);
  }
  if ((due & DRONE_FEED_PILOT) != 0) {
    feedPilot(entry);
  }
  if ((due & DRONE_FEED_POSITION) != 0) {
    feedPosition(entry);
  }
}

#endif
