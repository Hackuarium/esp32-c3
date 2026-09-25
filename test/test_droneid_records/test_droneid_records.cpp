#include <math.h>
#include <string.h>
#include <unity.h>

#include "droneIdRecords.h"

/* The three records a post puts on the mesh. The host decodes them from the
   hex body a bridge prints, so a byte in the wrong place here is an aircraft in
   the wrong place there - checked against exact bytes, not round trips through
   a decoder that would share the mistake.

   Runs on the host: `pio test -e native`. */

void setUp(void) {}

void tearDown(void) {}

/* What the host does: put back the whole 65536-unit windows from a position it
   already holds, which only has to be right to within +-0.327 degree. */
static double reconstruct(int16_t sent, double reference) {
  long referenceUnits = lround(reference * 1e5);
  int16_t delta = (int16_t)(uint16_t)((unsigned long)(sent - referenceUnits) &
                                      0xFFFF);
  return (referenceUnits + delta) / 1e5;
}

static DroneTrack fixtureTrack(void) {
  DroneTrack track;
  memset(&track, 0, sizeof(track));
  track.handle = 3;
  track.status = ODID_STATUS_AIRBORNE;
  track.first = true;
  track.latitude = 46.5197123;
  track.longitude = 6.6323011;
  track.height = 120.0f;
  track.speed = 12.5f;
  track.heading = 203.0f;
  track.rssi = -71;
  track.secondsSinceHeard = 2;
  track.transports = 0x02;
  return track;
}

static void test_track_bytes(void) {
  DroneTrack track = fixtureTrack();
  uint8_t body[48];
  size_t length = droneIdEncodeTrack(&track, 1, body, sizeof(body));

  const uint8_t expected[] = {0x10, 0x01, 0x03, 0x92, 0xC3, 0xFB, 0xBE,
                              0x1E, 0x3C, 0x19, 0x66, 0xB9, 0x22};
  TEST_ASSERT_EQUAL(sizeof(expected), length);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, body, sizeof(expected));
}

static void test_track_unknowns_and_limits(void) {
  DroneTrack track = fixtureTrack();
  track.first = false;
  track.conflict = true;
  track.heightAboveTakeoff = true;
  track.latitude = 0;
  track.longitude = 0;
  track.height = INV_ALT;
  track.speed = INV_SPEED_H;
  track.heading = INV_DIR;
  track.secondsSinceHeard = 40;
  uint8_t body[48];
  TEST_ASSERT_EQUAL(13, droneIdEncodeTrack(&track, 1, body, sizeof(body)));

  /* airborne, above takeoff, conflict - and no position bit */
  TEST_ASSERT_EQUAL_HEX8(0x2A, body[3]);
  TEST_ASSERT_EQUAL_HEX8(0x00, body[4]);
  TEST_ASSERT_EQUAL_HEX8(0x00, body[7]);
  TEST_ASSERT_EQUAL_HEX8(DRONE_TRACK_UNKNOWN, body[8]);
  TEST_ASSERT_EQUAL_HEX8(DRONE_TRACK_UNKNOWN, body[9]);
  TEST_ASSERT_EQUAL_HEX8(DRONE_TRACK_UNKNOWN, body[10]);
  /* the age nibble saturates at 15 */
  TEST_ASSERT_EQUAL_HEX8(0x2F, body[12]);

  track.height = -3.0f;
  track.speed = 200.0f;
  track.heading = 359.0f;
  droneIdEncodeTrack(&track, 1, body, sizeof(body));
  TEST_ASSERT_EQUAL_HEX8(0x00, body[8]);
  TEST_ASSERT_EQUAL_HEX8(254, body[9]);
  /* 359 degrees is closer to north than to 358 */
  TEST_ASSERT_EQUAL_HEX8(0x00, body[10]);
}

/* Four records is 46 bytes and fits; five does not, and nothing is written
   that the caller would then send cut short. */
static void test_track_frame_limit(void) {
  DroneTrack tracks[5];
  for (uint8_t i = 0; i < 5; i++) {
    tracks[i] = fixtureTrack();
    tracks[i].handle = i;
  }
  uint8_t body[48];
  TEST_ASSERT_EQUAL(46, droneIdEncodeTrack(tracks, 4, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(0x04, body[1]);
  TEST_ASSERT_EQUAL_HEX8(0x03, body[2 + 3 * DRONE_TRACK_RECORD_SIZE]);
  TEST_ASSERT_EQUAL(0, droneIdEncodeTrack(tracks, 5, body, sizeof(body)));
}

static void test_pilot_bytes(void) {
  DronePilot pilot = {3, ODID_OPERATOR_LOCATION_TYPE_LIVE_GNSS, true, false,
                      46.5190000, 6.6320000};
  uint8_t body[48];
  size_t length = droneIdEncodePilot(&pilot, 1, body, sizeof(body));

  const uint8_t expected[] = {0x11, 0x01, 0x03, 0x05,
                              0x7C, 0xFB, 0xA0, 0x1E};
  TEST_ASSERT_EQUAL(sizeof(expected), length);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, body, sizeof(expected));
}

static void test_ident_bytes(void) {
  char uas[ODID_ID_SIZE] = "1581F5559000000ABCD";
  char operatorId[ODID_ID_SIZE] = "FIN87astrdge12k8";
  DroneIdent ident = {3, ODID_IDTYPE_SERIAL_NUMBER,
                      ODID_UATYPE_HELICOPTER_OR_MULTIROTOR,
                      (ODID_CATEGORY_EU_OPEN << 4) | ODID_CLASS_EU_CLASS_1, uas,
                      operatorId};
  uint8_t body[48];
  size_t length = droneIdEncodeIdent(&ident, body, sizeof(body));

  TEST_ASSERT_EQUAL(5 + 19 + 1 + 16, length);
  const uint8_t head[] = {0x12, 0x03, 0x12, 0x12, 19};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(head, body, sizeof(head));
  TEST_ASSERT_EQUAL_MEMORY("1581F5559000000ABCD", body + 5, 19);
  TEST_ASSERT_EQUAL_HEX8(16, body[24]);
  TEST_ASSERT_EQUAL_MEMORY("FIN87astrdge12k8", body + 25, 16);

  /* before the Operator ID message has arrived */
  ident.operatorId = NULL;
  TEST_ASSERT_EQUAL(25, droneIdEncodeIdent(&ident, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(0x00, body[24]);
}

/* A binary identifier keeps its inner zeros; only the padding is trimmed. */
static void test_ident_binary_id(void) {
  char uas[ODID_ID_SIZE] = {0x12, 0x00, 0x34, 0x00, 0x56};
  DroneIdent ident = {0, ODID_IDTYPE_UTM_ASSIGNED_UUID, 0, 0, uas, NULL};
  uint8_t body[48];
  TEST_ASSERT_EQUAL(5 + 5 + 1, droneIdEncodeIdent(&ident, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(5, body[4]);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(uas, body + 5, 5);
}

/* The reference only has to be within the window: 0.3 degree off, and in
   either hemisphere, the coordinate comes back to the metre. */
static void test_coordinate_window(void) {
  int16_t sent = droneIdEncodeCoordinate(46.5197123);
  TEST_ASSERT_DOUBLE_WITHIN(0.000005, 46.5197123, reconstruct(sent, 46.8));
  TEST_ASSERT_DOUBLE_WITHIN(0.000005, 46.5197123, reconstruct(sent, 46.22));

  sent = droneIdEncodeCoordinate(-33.8688197);
  TEST_ASSERT_DOUBLE_WITHIN(0.000005, -33.8688197, reconstruct(sent, -34.1));
  sent = droneIdEncodeCoordinate(-0.00002);
  TEST_ASSERT_DOUBLE_WITHIN(0.000005, -0.00002, reconstruct(sent, 0.2));
}

static void test_metres_between(void) {
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 111.3,
                            droneIdMetresBetween(46.5, 6.6, 46.501, 6.6));
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 76.6,
                            droneIdMetresBetween(46.5, 6.6, 46.5, 6.601));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_track_bytes);
  RUN_TEST(test_track_unknowns_and_limits);
  RUN_TEST(test_track_frame_limit);
  RUN_TEST(test_pilot_bytes);
  RUN_TEST(test_ident_bytes);
  RUN_TEST(test_ident_binary_id);
  RUN_TEST(test_coordinate_window);
  RUN_TEST(test_metres_between);
  return UNITY_END();
}
