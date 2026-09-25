#ifndef _DRONE_ID_RECORDS_H
#define _DRONE_ID_RECORDS_H

#include <opendroneid.h>
#include <stddef.h>
#include <stdint.h>

/* The three records a watching post puts on the mesh, and nothing else.

   One aircraft forwarded raw costs more than the whole duty cycle, so what
   travels is a summary: TRACK for what changes, PILOT for where the operator
   stands, IDENT for who the aircraft claims to be. Each is a DATA body behind
   an opcode of its own, and the envelope - encryption, relaying, the governor -
   is the mesh's, unchanged. docs/drone-mesh-forwarding.md has the arithmetic.

   The firmware only ever encodes these. The bridge prints the decrypted body
   as hex on its (rx) line and the host decodes it, so there is one decoder and
   it is the one that can be re-run over a capture.

   Nothing here needs Arduino or a radio, so it is tested on the host. */

#define DRONE_MESH_OPCODE_TRACK 0x10
#define DRONE_MESH_OPCODE_PILOT 0x11
#define DRONE_MESH_OPCODE_IDENT 0x12

#define DRONE_TRACK_RECORD_SIZE 11
#define DRONE_PILOT_RECORD_SIZE 6

/* TRACK flags, bits 2-0 being the ODID status */
#define DRONE_TRACK_FLAG_ABOVE_TAKEOFF 0x08
#define DRONE_TRACK_FLAG_POSITION 0x10
#define DRONE_TRACK_FLAG_CONFLICT 0x20
#define DRONE_TRACK_FLAG_URGENT 0x40
#define DRONE_TRACK_FLAG_FIRST 0x80

/* PILOT flags, bits 1-0 being the operator location source */
#define DRONE_PILOT_FLAG_MOVED 0x04
#define DRONE_PILOT_FLAG_URGENT 0x08
#define DRONE_PILOT_SOURCE_UNKNOWN 3

/* The one-byte "unknown" of the quantized TRACK fields */
#define DRONE_TRACK_UNKNOWN 255

typedef struct {
  /* local to the sending node, bound to a UAS ID by IDENT */
  uint8_t handle;
  /* ODID_status_t, 0 to 4 */
  uint8_t status;
  bool heightAboveTakeoff;
  bool conflict;
  bool urgent;
  bool first;
  /* both 0 is "no position", as in Remote ID itself */
  double latitude;
  double longitude;
  /* metres; INV_ALT or below is unknown */
  float height;
  /* m/s; INV_SPEED_H or above is unknown */
  float speed;
  /* degrees; INV_DIR or above is unknown */
  float heading;
  int8_t rssi;
  uint32_t secondsSinceHeard;
  /* bit n set = heard on DRONE_SOURCE n */
  uint8_t transports;
} DroneTrack;

typedef struct {
  uint8_t handle;
  /* ODID_operator_location_type_t, or DRONE_PILOT_SOURCE_UNKNOWN */
  uint8_t source;
  bool moved;
  bool urgent;
  double latitude;
  double longitude;
} DronePilot;

typedef struct {
  uint8_t handle;
  uint8_t idType;
  uint8_t uaType;
  /* EU category in the high nibble and class in the low one, as on the wire;
     0 when the aircraft declared no EU classification */
  uint8_t classification;
  /* ODID_ID_SIZE bytes, NUL padded */
  const char* uasId;
  /* ODID_ID_SIZE bytes, NUL padded, or NULL before the Operator ID arrived */
  const char* operatorId;
} DroneIdent;

/* A coordinate in units of 1e-5 degree, kept modulo 65536. The host adds back
   the whole windows from the position it holds for the reporting node, so the
   reference only has to be right to within +-0.327 degree. */
int16_t droneIdEncodeCoordinate(double degrees);

/* opcode(1) count(1) records(count x 11). Returns the body length, or 0 when
   it does not fit in outSize. */
size_t droneIdEncodeTrack(const DroneTrack* tracks,
                          uint8_t count,
                          uint8_t* out,
                          size_t outSize);

/* opcode(1) count(1) records(count x 6). Returns the body length, or 0. */
size_t droneIdEncodePilot(const DronePilot* pilots,
                          uint8_t count,
                          uint8_t* out,
                          size_t outSize);

/* opcode(1) handle(1) types(1) class(1) uasLen(1) uas opLen(1) operator, both
   identifiers with their trailing NULs trimmed. Returns the body length, or 0. */
size_t droneIdEncodeIdent(const DroneIdent* ident, uint8_t* out, size_t outSize);

/* Equirectangular, exact enough for the tens to hundreds of metres it is asked
   about - a pilot walking, two transports disagreeing - at the cost of one
   cosine. */
double droneIdMetresBetween(double fromLatitude,
                            double fromLongitude,
                            double toLatitude,
                            double toLongitude);

#endif
