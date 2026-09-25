#include "droneIdRecords.h"

#include <math.h>
#include <string.h>

static void writeInt16(uint8_t* out, int16_t value) {
  /* little-endian, like every int16 value in a parameter body */
  out[0] = (uint8_t)((uint16_t)value & 0xFF);
  out[1] = (uint8_t)((uint16_t)value >> 8);
}

/* A value in `step` units, 0 to 254, with 255 left for unknown. Negative values
   clamp to 0: a height of -3 m is an aircraft on the ground with a noisy
   barometer, not one underground. */
static uint8_t quantize(double value, double step) {
  if (value <= 0) {
    return 0;
  }
  long steps = lround(value / step);
  return steps > 254 ? 254 : (uint8_t)steps;
}

static bool hasPosition(double latitude, double longitude) {
  return latitude != 0 || longitude != 0;
}

static uint8_t trimmedLength(const char* text) {
  uint8_t length = ODID_ID_SIZE;
  while (length > 0 && text[length - 1] == '\0') {
    length--;
  }
  return length;
}

int16_t droneIdEncodeCoordinate(double degrees) {
  long units = lround(degrees * 1e5);
  return (int16_t)(uint16_t)((unsigned long)units & 0xFFFF);
}

size_t droneIdEncodeTrack(const DroneTrack* tracks,
                          uint8_t count,
                          uint8_t* out,
                          size_t outSize) {
  size_t length = 2 + (size_t)count * DRONE_TRACK_RECORD_SIZE;
  if (length > outSize) {
    return 0;
  }
  out[0] = DRONE_MESH_OPCODE_TRACK;
  out[1] = count;
  for (uint8_t i = 0; i < count; i++) {
    const DroneTrack* track = &tracks[i];
    uint8_t* record = out + 2 + (size_t)i * DRONE_TRACK_RECORD_SIZE;
    bool located = hasPosition(track->latitude, track->longitude);

    uint8_t flags = track->status & 0x07;
    if (track->heightAboveTakeoff) flags |= DRONE_TRACK_FLAG_ABOVE_TAKEOFF;
    if (located) flags |= DRONE_TRACK_FLAG_POSITION;
    if (track->conflict) flags |= DRONE_TRACK_FLAG_CONFLICT;
    if (track->urgent) flags |= DRONE_TRACK_FLAG_URGENT;
    if (track->first) flags |= DRONE_TRACK_FLAG_FIRST;

    record[0] = track->handle;
    record[1] = flags;
    writeInt16(record + 2,
               located ? droneIdEncodeCoordinate(track->latitude) : 0);
    writeInt16(record + 4,
               located ? droneIdEncodeCoordinate(track->longitude) : 0);
    record[6] = track->height <= INV_ALT ? DRONE_TRACK_UNKNOWN
                                         : quantize(track->height, 2.0);
    record[7] = track->speed >= INV_SPEED_H ? DRONE_TRACK_UNKNOWN
                                            : quantize(track->speed, 0.5);
    /* 180 steps of 2 degrees, so 359 rounds to 0 rather than to a 180th */
    record[8] = track->heading >= INV_DIR || track->heading < 0
                    ? DRONE_TRACK_UNKNOWN
                    : (uint8_t)(lround(track->heading / 2.0) % 180);
    record[9] = (uint8_t)track->rssi;
    uint8_t age =
        track->secondsSinceHeard > 15 ? 15 : (uint8_t)track->secondsSinceHeard;
    record[10] = (uint8_t)((track->transports & 0x0F) << 4) | age;
  }
  return length;
}

size_t droneIdEncodePilot(const DronePilot* pilots,
                          uint8_t count,
                          uint8_t* out,
                          size_t outSize) {
  size_t length = 2 + (size_t)count * DRONE_PILOT_RECORD_SIZE;
  if (length > outSize) {
    return 0;
  }
  out[0] = DRONE_MESH_OPCODE_PILOT;
  out[1] = count;
  for (uint8_t i = 0; i < count; i++) {
    const DronePilot* pilot = &pilots[i];
    uint8_t* record = out + 2 + (size_t)i * DRONE_PILOT_RECORD_SIZE;
    uint8_t flags = pilot->source > DRONE_PILOT_SOURCE_UNKNOWN
                        ? DRONE_PILOT_SOURCE_UNKNOWN
                        : pilot->source;
    if (pilot->moved) flags |= DRONE_PILOT_FLAG_MOVED;
    if (pilot->urgent) flags |= DRONE_PILOT_FLAG_URGENT;
    record[0] = pilot->handle;
    record[1] = flags;
    writeInt16(record + 2, droneIdEncodeCoordinate(pilot->latitude));
    writeInt16(record + 4, droneIdEncodeCoordinate(pilot->longitude));
  }
  return length;
}

size_t droneIdEncodeIdent(const DroneIdent* ident, uint8_t* out, size_t outSize) {
  uint8_t uasLength = trimmedLength(ident->uasId);
  uint8_t operatorLength =
      ident->operatorId == NULL ? 0 : trimmedLength(ident->operatorId);
  size_t length = 5 + uasLength + 1 + operatorLength;
  if (length > outSize) {
    return 0;
  }
  uint8_t* at = out;
  *at++ = DRONE_MESH_OPCODE_IDENT;
  *at++ = ident->handle;
  *at++ = (uint8_t)(((ident->idType & 0x0F) << 4) | (ident->uaType & 0x0F));
  *at++ = ident->classification;
  *at++ = uasLength;
  memcpy(at, ident->uasId, uasLength);
  at += uasLength;
  *at++ = operatorLength;
  if (operatorLength > 0) {
    memcpy(at, ident->operatorId, operatorLength);
  }
  return length;
}

double droneIdMetresBetween(double fromLatitude,
                            double fromLongitude,
                            double toLatitude,
                            double toLongitude) {
  const double metresPerDegree = 111320.0;
  double latitude = (toLatitude - fromLatitude) * metresPerDegree;
  double longitude = (toLongitude - fromLongitude) * metresPerDegree *
                     cos(fromLatitude * M_PI / 180.0);
  return sqrt(latitude * latitude + longitude * longitude);
}
