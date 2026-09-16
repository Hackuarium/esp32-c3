#include <string.h>
#include <unity.h>

#include "droneIdFrames.h"
#include "fixtures.h"

/* Finding the Open Drone ID payload inside a frame is the part of this board
   that is easy to get wrong by one byte and impossible to notice: an offset
   that is one out decodes a valid message as garbage, or - worse - decodes
   garbage as a position and puts an aircraft somewhere it is not.

   So it is tested against real frames from the reference transmitter, through
   the same functions the firmware calls, and the decoded values are checked
   against what was encoded rather than against "it returned something".

   Runs on the host: `pio test -e native`. */

static ODID_UAS_Data record;

void setUp(void) { odid_initUasData(&record); }

void tearDown(void) {}

/* Everything the fixture aircraft said, so a locator that finds the right
   bytes but slices them one out is caught by the values rather than by the
   pointer. */
static void assertFixtureAircraft(void) {
  TEST_ASSERT_TRUE(record.BasicIDValid[0]);
  TEST_ASSERT_EQUAL_STRING("1581F5559000000ABCD", record.BasicID[0].UASID);
  TEST_ASSERT_EQUAL_UINT8(ODID_IDTYPE_SERIAL_NUMBER, record.BasicID[0].IDType);
  TEST_ASSERT_EQUAL_UINT8(ODID_UATYPE_HELICOPTER_OR_MULTIROTOR,
                          record.BasicID[0].UAType);

  TEST_ASSERT_TRUE(record.LocationValid);
  TEST_ASSERT_EQUAL_UINT8(ODID_STATUS_AIRBORNE, record.Location.Status);
  /* a hundred nanodegrees, which is the resolution the wire has */
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 46.5197123, record.Location.Latitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 6.6323011, record.Location.Longitude);
  TEST_ASSERT_EQUAL_FLOAT(512.0f, record.Location.AltitudeGeo);
  TEST_ASSERT_EQUAL_FLOAT(120.0f, record.Location.Height);
  TEST_ASSERT_EQUAL_UINT8(ODID_HEIGHT_REF_OVER_GROUND,
                          record.Location.HeightType);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, record.Location.SpeedHorizontal);
  TEST_ASSERT_EQUAL_FLOAT(1.5f, record.Location.SpeedVertical);
  /* 203 is past 180, so it travelled as 23 with the east-west bit set */
  TEST_ASSERT_EQUAL_FLOAT(203.0f, record.Location.Direction);

  TEST_ASSERT_TRUE(record.SystemValid);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 46.5190000,
                            record.System.OperatorLatitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 6.6320000,
                            record.System.OperatorLongitude);
  /* the wire value is the class number plus one: C1 travels as 2 */
  TEST_ASSERT_EQUAL_UINT8(ODID_CLASS_EU_CLASS_1, record.System.ClassEU);
  TEST_ASSERT_EQUAL_UINT8(ODID_CATEGORY_EU_OPEN, record.System.CategoryEU);
}

/* Exactly 31 bytes, one message, and the service data fills the whole legacy
   payload - which is why a compliant advertisement carries no AD Flags. */
static void test_bluetooth_legacy(void) {
  uint8_t length = 0;
  const uint8_t* payload = droneIdFindBluetoothPayload(
      bluetoothLegacy, sizeof(bluetoothLegacy), &length);

  TEST_ASSERT_NOT_NULL(payload);
  TEST_ASSERT_EQUAL_PTR(&bluetoothLegacy[5], payload);
  TEST_ASSERT_EQUAL_UINT8(0x2A, payload[0]);
  TEST_ASSERT_EQUAL_UINT8(1 + ODID_MESSAGE_SIZE, length);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_BASIC_ID,
                    droneIdDecodePayload(payload, length, &record));
  TEST_ASSERT_TRUE(record.BasicIDValid[0]);
  TEST_ASSERT_EQUAL_STRING("1581F5559000000ABCD", record.BasicID[0].UASID);
}

/* The structures are walked rather than indexed at offset 6. An extended
   advertisement has room to put Flags or TX Power ahead of the service data,
   and the reference Android receiver's fixed offset misses exactly this. */
static void test_bluetooth_pack_behind_flags(void) {
  uint8_t length = 0;
  const uint8_t* payload = droneIdFindBluetoothPayload(
      bluetoothExtended, sizeof(bluetoothExtended), &length);

  TEST_ASSERT_NOT_NULL(payload);
  TEST_ASSERT_EQUAL_UINT8(0x99, payload[0]);
  TEST_ASSERT_EQUAL_UINT8(ODID_MESSAGETYPE_PACKED, payload[1] >> 4);
  TEST_ASSERT_EQUAL_UINT8(ODID_MESSAGE_SIZE, payload[2]);
  TEST_ASSERT_EQUAL_UINT8(3, payload[3]);
  TEST_ASSERT_EQUAL_UINT8(1 + 3 + 3 * ODID_MESSAGE_SIZE, length);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_PACKED,
                    droneIdDecodePayload(payload, length, &record));
  assertFixtureAircraft();
}

/* What a split extended advertisement leaves behind: a pack header claiming
   more messages than arrived. Half a pack decodes into a position belonging to
   no message, so it is refused whole. */
static void test_truncated_pack_is_refused(void) {
  uint8_t truncated[sizeof(bluetoothExtended)];
  memcpy(truncated, bluetoothExtended, sizeof(bluetoothExtended));
  size_t shortLength = sizeof(bluetoothExtended) - 30;
  truncated[3] = (uint8_t)(truncated[3] - 30);

  uint8_t length = 0;
  const uint8_t* payload =
      droneIdFindBluetoothPayload(truncated, shortLength, &length);
  if (payload != NULL) {
    TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_INVALID,
                      droneIdDecodePayload(payload, length, &record));
  }
  TEST_ASSERT_FALSE(record.LocationValid);
}

/* The information elements start after the 24-byte management header and the
   12 fixed bytes of timestamp, beacon interval and capability. */
static void test_wifi_beacon(void) {
  uint8_t length = 0;
  const uint8_t* payload =
      droneIdFindBeaconPayload(beaconFrame, sizeof(beaconFrame), &length);

  TEST_ASSERT_NOT_NULL(payload);
  TEST_ASSERT_EQUAL_UINT8(0x55, payload[0]);
  TEST_ASSERT_EQUAL_UINT8(ODID_MESSAGETYPE_PACKED, payload[1] >> 4);
  TEST_ASSERT_EQUAL_UINT8(1 + 3 + payload[3] * ODID_MESSAGE_SIZE, length);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_PACKED,
                    droneIdDecodePayload(payload, length, &record));
  assertFixtureAircraft();
  TEST_ASSERT_TRUE(record.OperatorIDValid);
  TEST_ASSERT_EQUAL_STRING("FIN87astrdge12k8", record.OperatorID.OperatorId);
}

/* An element length that runs off the end makes every element after it
   unfindable, so the walk stops rather than reading past the buffer. */
static void test_truncated_beacon_is_refused(void) {
  uint8_t length = 0;
  TEST_ASSERT_NULL(droneIdFindBeaconPayload(
      beaconFrame, (uint16_t)(sizeof(beaconFrame) - 10), &length));
}

/* A service discovery frame has no elements to walk, so every field is matched
   at a fixed offset - through the Open Drone ID service id, which is the first
   six bytes of the SHA-256 of "org.opendroneid.remoteid". */
static void test_wifi_nan(void) {
  uint8_t length = 0;
  const uint8_t* payload =
      droneIdFindNanPayload(nanFrame, sizeof(nanFrame), &length);

  TEST_ASSERT_NOT_NULL(payload);
  TEST_ASSERT_EQUAL_UINT8(0x77, payload[0]);
  TEST_ASSERT_EQUAL_UINT8(ODID_MESSAGETYPE_PACKED, payload[1] >> 4);
  TEST_ASSERT_EQUAL_UINT8(1 + 3 + payload[3] * ODID_MESSAGE_SIZE, length);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_PACKED,
                    droneIdDecodePayload(payload, length, &record));
  assertFixtureAircraft();
}

/* The two Wi-Fi locators are chosen by frame subtype, so neither may accept
   the other's frame if that choice is ever got wrong. */
static void test_locators_do_not_cross_match(void) {
  uint8_t length = 0;
  TEST_ASSERT_NULL(
      droneIdFindBeaconPayload(nanFrame, sizeof(nanFrame), &length));
  TEST_ASSERT_NULL(
      droneIdFindNanPayload(beaconFrame, sizeof(beaconFrame), &length));
}

/* Both radios hand over whatever was on the air, so every locator is fed
   rubbish on every sweep and must simply say no. */
static void test_noise_matches_nothing(void) {
  uint8_t junk[64];
  for (size_t i = 0; i < sizeof(junk); i++) {
    junk[i] = (uint8_t)(i * 7 + 3);
  }
  uint8_t length = 0;
  TEST_ASSERT_NULL(droneIdFindBluetoothPayload(junk, sizeof(junk), &length));
  TEST_ASSERT_NULL(droneIdFindBeaconPayload(junk, sizeof(junk), &length));
  TEST_ASSERT_NULL(droneIdFindNanPayload(junk, sizeof(junk), &length));

  uint8_t empty[1] = {0};
  TEST_ASSERT_NULL(droneIdFindBluetoothPayload(empty, 0, &length));
  TEST_ASSERT_NULL(droneIdFindBeaconPayload(empty, 0, &length));
  TEST_ASSERT_NULL(droneIdFindNanPayload(empty, 0, &length));
}

/* A payload shorter than one message never reaches the decoder, which takes no
   length of its own and would read past the end of the radio's buffer. */
static void test_short_payload_never_reaches_the_decoder(void) {
  uint8_t payload[8] = {0x2A, 0x02};
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_INVALID,
                    droneIdDecodePayload(payload, sizeof(payload), &record));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_bluetooth_legacy);
  RUN_TEST(test_bluetooth_pack_behind_flags);
  RUN_TEST(test_truncated_pack_is_refused);
  RUN_TEST(test_wifi_beacon);
  RUN_TEST(test_truncated_beacon_is_refused);
  RUN_TEST(test_wifi_nan);
  RUN_TEST(test_locators_do_not_cross_match);
  RUN_TEST(test_noise_matches_nothing);
  RUN_TEST(test_short_payload_never_reaches_the_decoder);
  return UNITY_END();
}
