#include "droneIdTransmit.h"

#include <math.h>
#include <string.h>

/* No config.h, for the reason droneIdFrames.cpp gives: the host test builds
   this file too. */

#define BLUETOOTH_AD_SERVICE_DATA_16 0x16
#define ODID_UUID_LOW 0xFA
#define ODID_UUID_HIGH 0xFF
#define ODID_APPLICATION_CODE 0x0D

static const uint8_t astmOui[3] = {0xFA, 0x0B, 0xBC};
#define WIFI_ELEMENT_SSID 0x00
#define WIFI_ELEMENT_RATES 0x01
#define WIFI_ELEMENT_VENDOR_SPECIFIC 0xDD
#define WIFI_ODID_VENDOR_TYPE 0x0D
/* short preamble and short slot time, as upstream's wifi.c sets them */
#define WIFI_BEACON_CAPABILITY 0x0420
/* one basic rate, 6 Mbit/s: an element a scanner expects to find */
#define WIFI_BASIC_RATE 0x8C
#define WIFI_SSID_MAX 32

/* the order a pack carries them in, identity first */
static const ODID_messagetype_t packOrder[] = {
    ODID_MESSAGETYPE_BASIC_ID, ODID_MESSAGETYPE_LOCATION,
    ODID_MESSAGETYPE_SELF_ID, ODID_MESSAGETYPE_SYSTEM,
    ODID_MESSAGETYPE_OPERATOR_ID};

static bool encodeMessage(const ODID_UAS_Data* record,
                          ODID_messagetype_t type,
                          ODID_Message_encoded* out) {
  switch (type) {
    case ODID_MESSAGETYPE_BASIC_ID:
      return record->BasicIDValid[0] &&
             encodeBasicIDMessage(&out->basicId, &record->BasicID[0]) ==
                 ODID_SUCCESS;
    case ODID_MESSAGETYPE_LOCATION:
      return record->LocationValid &&
             encodeLocationMessage(&out->location, &record->Location) ==
                 ODID_SUCCESS;
    case ODID_MESSAGETYPE_SELF_ID:
      return record->SelfIDValid &&
             encodeSelfIDMessage(&out->selfId, &record->SelfID) ==
                 ODID_SUCCESS;
    case ODID_MESSAGETYPE_SYSTEM:
      return record->SystemValid &&
             encodeSystemMessage(&out->system, &record->System) ==
                 ODID_SUCCESS;
    case ODID_MESSAGETYPE_OPERATOR_ID:
      return record->OperatorIDValid &&
             encodeOperatorIDMessage(&out->operatorId, &record->OperatorID) ==
                 ODID_SUCCESS;
    default:
      return false;
  }
}

/* The pack header and only the messages it holds: encodeMessagePack() fills a
   228-byte struct, of which a five-message pack uses 128. */
static size_t buildPack(const ODID_UAS_Data* record,
                        uint8_t* out,
                        size_t capacity) {
  ODID_MessagePack_data pack;
  odid_initMessagePackData(&pack);
  for (size_t i = 0; i < sizeof(packOrder) / sizeof(packOrder[0]); i++) {
    if (encodeMessage(record, packOrder[i], &pack.Messages[pack.MsgPackSize])) {
      pack.MsgPackSize++;
    }
  }
  size_t length = 3 + (size_t)ODID_MESSAGE_SIZE * pack.MsgPackSize;
  ODID_MessagePack_encoded encoded;
  if (pack.MsgPackSize == 0 || length > capacity ||
      encodeMessagePack(&encoded, &pack) != ODID_SUCCESS) {
    return 0;
  }
  memcpy(out, &encoded, length);
  return length;
}

/* length, type, UUID and application code: what precedes the counter */
static void writeServiceDataHeader(uint8_t* out, uint8_t bodyLength) {
  out[0] = (uint8_t)(4 + bodyLength);
  out[1] = BLUETOOTH_AD_SERVICE_DATA_16;
  out[2] = ODID_UUID_LOW;
  out[3] = ODID_UUID_HIGH;
  out[4] = ODID_APPLICATION_CODE;
}

size_t droneIdBuildLegacyAdvertisement(const ODID_UAS_Data* record,
                                       ODID_messagetype_t type,
                                       uint8_t counter,
                                       uint8_t* out) {
  ODID_Message_encoded message;
  if (!encodeMessage(record, type, &message)) {
    return 0;
  }
  writeServiceDataHeader(out, 1 + ODID_MESSAGE_SIZE);
  out[5] = counter;
  memcpy(&out[6], message.rawData, ODID_MESSAGE_SIZE);
  return DRONE_LEGACY_ADVERTISEMENT_LENGTH;
}

size_t droneIdBuildExtendedAdvertisement(const ODID_UAS_Data* record,
                                         uint8_t counter,
                                         uint8_t* out,
                                         size_t capacity) {
  if (capacity < 6) {
    return 0;
  }
  size_t packLength = buildPack(record, &out[6], capacity - 6);
  if (packLength == 0) {
    return 0;
  }
  writeServiceDataHeader(out, (uint8_t)(1 + packLength));
  out[5] = counter;
  return 6 + packLength;
}

static size_t put16(uint8_t* out, size_t at, uint16_t value) {
  out[at] = (uint8_t)(value & 0xFF);
  out[at + 1] = (uint8_t)(value >> 8);
  return at + 2;
}

size_t droneIdBuildBeaconFrame(const ODID_UAS_Data* record,
                               const uint8_t address[6],
                               const char* ssid,
                               uint16_t intervalTu,
                               uint8_t counter,
                               uint8_t* out,
                               size_t capacity) {
  size_t ssidLength = strlen(ssid);
  if (ssidLength > WIFI_SSID_MAX) {
    ssidLength = WIFI_SSID_MAX;
  }
  /* header 24, fixed fields 12, SSID, rates 3, vendor element up to the pack */
  size_t head = 24 + 12 + 2 + ssidLength + 3 + 2 + 4 + 1;
  if (capacity <= head) {
    return 0;
  }
  size_t packLength = buildPack(record, &out[head], capacity - head);
  if (packLength == 0) {
    return 0;
  }

  size_t at = put16(out, 0, 0x0080);  // management, beacon
  at = put16(out, at, 0);             // duration
  memset(&out[at], 0xFF, 6);          // to everyone
  at += 6;
  memcpy(&out[at], address, 6);  // source
  at += 6;
  memcpy(&out[at], address, 6);  // BSSID
  at += 6;
  at = put16(out, at, 0);  // sequence, replaced by the driver
  memset(&out[at], 0, 8);  // timestamp, this transmitter has no TSF
  at += 8;
  at = put16(out, at, intervalTu);
  at = put16(out, at, WIFI_BEACON_CAPABILITY);

  out[at++] = WIFI_ELEMENT_SSID;
  out[at++] = (uint8_t)ssidLength;
  memcpy(&out[at], ssid, ssidLength);
  at += ssidLength;

  out[at++] = WIFI_ELEMENT_RATES;
  out[at++] = 1;
  out[at++] = WIFI_BASIC_RATE;

  out[at++] = WIFI_ELEMENT_VENDOR_SPECIFIC;
  out[at++] = (uint8_t)(4 + 1 + packLength);
  memcpy(&out[at], astmOui, sizeof(astmOui));
  at += sizeof(astmOui);
  out[at++] = WIFI_ODID_VENDOR_TYPE;
  out[at++] = counter;
  return at + packLength;
}

void droneIdOrbitPosition(double centerLatitude,
                          double centerLongitude,
                          double radiusMetres,
                          double speed,
                          double seconds,
                          double* latitude,
                          double* longitude,
                          float* course) {
  const double metresPerDegree = 111320.0;
  /* the phase is taken modulo one lap before it becomes an angle, so it stays
     exact however long the board has been running */
  double lap = 2 * M_PI * radiusMetres / speed;
  double bearing = 2 * M_PI * fmod(seconds, lap) / lap;
  double north = radiusMetres * cos(bearing);
  double east = radiusMetres * sin(bearing);
  *latitude = centerLatitude + north / metresPerDegree;
  *longitude = centerLongitude + east / (metresPerDegree *
                                         cos(centerLatitude * M_PI / 180.0));
  /* clockwise, so the course is the bearing from the centre plus a right
     angle */
  double degrees = fmod(bearing * 180.0 / M_PI + 90.0, 360.0);
  *course = (float)degrees < 360.0f ? (float)degrees : 0.0f;
}
