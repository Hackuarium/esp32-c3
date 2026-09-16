#include "config.h"
#ifdef THR_DRONE_ID
#include <string.h>

#include "droneIdDecode.h"
#include "droneIdLabels.h"
#include "droneIdReport.h"

void droneIdPrintAddress(Print* output, const uint8_t* address) {
  for (uint8_t i = 0; i < DRONE_ADDRESS_LENGTH; i++) {
    if (i > 0) {
      output->print(':');
    }
    if (address[i] < 0x10) {
      output->print('0');
    }
    output->print(address[i], HEX);
  }
}

const __FlashStringHelper* droneIdSourceLabel(uint8_t source) {
  switch (source) {
    case DRONE_SOURCE_BLE_LEGACY:
      return F("BT4");
    case DRONE_SOURCE_BLE_EXTENDED:
      return F("BT5");
    case DRONE_SOURCE_WIFI_BEACON:
      return F("beacon");
    default:
      return F("NAN");
  }
}

static void printCoordinates(Print* output, double latitude, double longitude) {
  output->print(latitude, 7);
  output->print(F(", "));
  output->print(longitude, 7);
}

/* The UAS ID when the aircraft has given one that is text, the address when it
   has not. An identifier a spoofer chose is still what an operator reads
   first; the address is on the line under it. */
static void printName(Print* output, const DroneAircraft* entry) {
  const ODID_BasicID_data* basic = droneIdDecodeBasicId(&entry->record);
  if (basic != NULL && droneIdDecodeIdIsText(basic->IDType)) {
    output->print(basic->UASID);
    return;
  }
  droneIdPrintAddress(output, entry->address);
}

static void printSource(Print* output, const DroneAircraft* entry) {
  output->print(droneIdSourceLabel(entry->source));
  if (entry->channel > 0) {
    output->print(F(" ch"));
    output->print(entry->channel);
  }
}

static void printAge(Print* output, uint32_t sinceMillis) {
  output->print((millis() - sinceMillis) / 1000);
  output->print(F(" s ago"));
}

static void printPosition(Print* output, const DroneAircraft* entry) {
  const ODID_Location_data* location = &entry->record.Location;
  if (entry->record.LocationValid &&
      droneIdDecodeHasPosition(location->Latitude, location->Longitude)) {
    printCoordinates(output, location->Latitude, location->Longitude);
  } else {
    output->print(F("no position"));
  }
}

static void printIdentity(Print* output, const ODID_UAS_Data* record) {
  for (uint8_t i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
    if (!record->BasicIDValid[i]) {
      continue;
    }
    const ODID_BasicID_data* basic = &record->BasicID[i];
    output->print(F("UAS ID: "));
    if (droneIdDecodeIdIsText(basic->IDType)) {
      output->print(basic->UASID);
    } else {
      /* A UTM UUID or a session id is 20 raw bytes, and printing it as a
         string would stop at its first zero and invent a shorter aircraft. */
      for (uint8_t at = 0; at < ODID_ID_SIZE; at++) {
        uint8_t value = (uint8_t)basic->UASID[at];
        if (value < 0x10) {
          output->print('0');
        }
        output->print(value, HEX);
      }
    }
    output->print(F(" ("));
    output->print(droneIdIdTypeLabel(basic->IDType));
    output->print(F("), "));
    output->println(droneIdUaTypeLabel(basic->UAType));
  }
}

static void printFlight(Print* output, const ODID_Location_data* location) {
  output->print(F("State: "));
  output->println(droneIdStatusLabel(location->Status));

  if (droneIdDecodeHasPosition(location->Latitude, location->Longitude)) {
    output->print(F("Position: "));
    printCoordinates(output, location->Latitude, location->Longitude);
    output->print(F(" ("));
    droneIdPrintHorizontalAccuracy(output, location->HorizAccuracy);
    output->println(')');
  }

  if (location->AltitudeGeo > INV_ALT || location->Height > INV_ALT) {
    output->print(F("Altitude: "));
    if (location->AltitudeGeo > INV_ALT) {
      output->print(location->AltitudeGeo, 1);
      output->print(F(" m geodetic"));
    }
    if (location->Height > INV_ALT) {
      if (location->AltitudeGeo > INV_ALT) {
        output->print(F(", "));
      }
      output->print(location->Height, 1);
      output->print(location->HeightType == ODID_HEIGHT_REF_OVER_GROUND
                        ? F(" m above ground")
                        : F(" m above takeoff"));
    }
    output->print(F(" ("));
    droneIdPrintVerticalAccuracy(output, location->VertAccuracy);
    output->println(')');
  }

  if (location->SpeedHorizontal < INV_SPEED_H) {
    output->print(F("Movement: "));
    output->print(location->SpeedHorizontal, 2);
    output->print(F(" m/s"));
    if (location->Direction < INV_DIR) {
      output->print(F(", heading "));
      output->print(location->Direction, 0);
    }
    if (location->SpeedVertical < INV_SPEED_V) {
      output->print(location->SpeedVertical < 0 ? F(", down ") : F(", up "));
      output->print(fabsf(location->SpeedVertical), 1);
      output->print(F(" m/s"));
    }
    output->print(F(" ("));
    droneIdPrintSpeedAccuracy(output, location->SpeedAccuracy);
    output->println(')');
  }
}

static void printOperator(Print* output, const ODID_UAS_Data* record) {
  if (record->SystemValid) {
    const ODID_System_data* system = &record->System;
    if (droneIdDecodeHasPosition(system->OperatorLatitude,
                                 system->OperatorLongitude)) {
      output->print(F("Operator: "));
      printCoordinates(output, system->OperatorLatitude,
                       system->OperatorLongitude);
      if (system->OperatorAltitudeGeo > INV_ALT) {
        output->print(F(", "));
        output->print(system->OperatorAltitudeGeo, 0);
        output->print(F(" m"));
      }
      output->print(F(" ("));
      output->print(droneIdOperatorLocationLabel(system->OperatorLocationType));
      output->println(')');
    }
    if (system->ClassificationType == ODID_CLASSIFICATION_TYPE_EU) {
      output->print(F("Class: EU "));
      output->print(droneIdCategoryLabel(system->CategoryEU));
      output->print(' ');
      output->println(droneIdClassLabel(system->ClassEU));
    }
    if (system->AreaCount > 1) {
      output->print(F("Swarm: "));
      output->print(system->AreaCount);
      output->print(F(" aircraft within "));
      output->print(system->AreaRadius);
      output->println(F(" m"));
    }
  }
  if (record->OperatorIDValid) {
    output->print(F("Operator ID: "));
    output->println(record->OperatorID.OperatorId);
  }
  if (record->SelfIDValid) {
    output->print(F("Self ID: "));
    output->println(record->SelfID.Desc);
  }
  for (uint8_t i = 0; i < ODID_AUTH_MAX_PAGES; i++) {
    if (record->AuthValid[i] && record->Auth[i].DataPage == 0) {
      /* The signature is only checkable against a registry this board cannot
         reach, so what is reported is that one was offered. */
      output->print(F("Signed: "));
      output->println(droneIdAuthTypeLabel(record->Auth[i].AuthType));
      break;
    }
  }
}

void droneIdReportAircraft(Print* output, const DroneAircraft* entry,
                           boolean isNew) {
  output->print(isNew ? F("=== Drone heard, ") : F("=== Drone, more from "));
  printSource(output, entry);
  output->println(F(" ==="));
  output->print(F("Address: "));
  droneIdPrintAddress(output, entry->address);
  output->print(F(", "));
  output->print(entry->rssi);
  output->println(F(" dBm"));
  printIdentity(output, &entry->record);
  if (entry->record.LocationValid) {
    printFlight(output, &entry->record.Location);
  }
  printOperator(output, &entry->record);
}

void droneIdReportPosition(Print* output, const DroneAircraft* entry) {
  const ODID_Location_data* location = &entry->record.Location;
  output->print(F("[drone] "));
  printName(output, entry);
  output->print(' ');
  printPosition(output, entry);
  if (entry->record.LocationValid) {
    if (location->AltitudeGeo > INV_ALT) {
      output->print(' ');
      output->print(location->AltitudeGeo, 0);
      output->print(F(" m"));
    }
    if (location->SpeedHorizontal < INV_SPEED_H) {
      output->print(' ');
      output->print(location->SpeedHorizontal, 1);
      output->print(F(" m/s"));
    }
  }
  output->print(' ');
  output->print(entry->rssi);
  output->print(F(" dBm "));
  printSource(output, entry);
  output->println();
}

void droneIdReportLost(Print* output, const DroneAircraft* entry) {
  output->print(F("[drone] lost "));
  printName(output, entry);
  output->print(F(", last heard "));
  printAge(output, entry->lastSeenMillis);
  output->print(F(" after "));
  output->print(entry->messages);
  output->println(F(" frames"));
}

void droneIdReportList(Print* output) {
  static DroneAircraft entry;
  uint8_t listed = 0;
  for (uint8_t i = 0; i < DRONE_MAX_AIRCRAFT; i++) {
    if (!droneIdTableCopy(i, &entry)) {
      continue;
    }
    listed++;
    output->print(i);
    output->print(' ');
    printName(output, &entry);
    output->print(' ');
    printSource(output, &entry);
    output->print(' ');
    output->print(entry.rssi);
    output->print(F(" dBm "));
    printPosition(output, &entry);
    output->print(F(", "));
    printAge(output, entry.lastSeenMillis);
    /* The same aircraft on two radios is two transmitters with two addresses,
       so it is two rows. Saying which rows agree on a UAS ID is the honest
       version of merging them. */
    int16_t twin = droneIdTableTwin(i);
    if (twin >= 0) {
      output->print(F(", same UAS ID as "));
      output->print(twin);
    }
    output->println();
  }
  if (listed == 0) {
    output->println(F("No drone heard"));
  }
}

void droneIdReportDetail(Print* output, uint8_t index) {
  static DroneAircraft entry;
  if (!droneIdTableCopy(index, &entry)) {
    output->println(F("No such drone, dl lists them"));
    return;
  }
  droneIdReportAircraft(output, &entry, false);
  const ODID_Location_data* location = &entry.record.Location;
  if (entry.record.LocationValid && location->TimeStamp <= MAX_TIMESTAMP) {
    /* Seconds since the last full hour is all an aircraft sends; which hour
       has to come from the reader's own clock, and this board has none it can
       trust. */
    output->print(F("Sent at: "));
    output->print(location->TimeStamp, 1);
    output->println(F(" s past the hour"));
  }
  output->print(F("Frames: "));
  output->print(entry.messages);
  output->print(F(", best "));
  output->print(entry.bestRssi);
  output->print(F(" dBm, first heard "));
  printAge(output, entry.firstSeenMillis);
  output->print(F(", protocol version "));
  output->println(entry.protocolVersion);
}
#endif
