#include <math.h>
#include <string.h>
#include <unity.h>

#include "droneIdFrames.h"
#include "droneIdRecords.h"
#include "droneIdTransmit.h"

/* The transmitter's frames, handed back to the receiver's own locators and
   decoder. A test transmitter that the watcher cannot read is worse than none:
   it would send somebody looking for a receiver fault into the wrong half.

   Runs on the host: `pio test -e native`. */

static ODID_UAS_Data aircraft;
static ODID_UAS_Data heard;
static uint8_t frame[256];

static const uint8_t address[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

void setUp(void) {
  odid_initUasData(&aircraft);
  odid_initUasData(&heard);

  aircraft.BasicIDValid[0] = 1;
  aircraft.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
  aircraft.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
  strcpy(aircraft.BasicID[0].UASID, "TESTC0123456789AB");

  aircraft.LocationValid = 1;
  aircraft.Location.Status = ODID_STATUS_AIRBORNE;
  aircraft.Location.Latitude = 46.5000000;
  aircraft.Location.Longitude = 6.6000000;
  aircraft.Location.AltitudeGeo = 459.0f;
  aircraft.Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
  aircraft.Location.Height = 10.0f;
  aircraft.Location.SpeedHorizontal = 0.0f;
  aircraft.Location.SpeedVertical = 0.0f;
  aircraft.Location.TimeStamp = INV_TIMESTAMP;

  aircraft.SelfIDValid = 1;
  strcpy(aircraft.SelfID.Desc, "hackuarium test");

  aircraft.SystemValid = 1;
  aircraft.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_FIXED;
  aircraft.System.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
  aircraft.System.OperatorLatitude = 46.5000000;
  aircraft.System.OperatorLongitude = 6.6000000;
  aircraft.System.OperatorAltitudeGeo = 449.0f;
  aircraft.System.CategoryEU = ODID_CATEGORY_EU_OPEN;
  aircraft.System.ClassEU = ODID_CLASS_EU_CLASS_0;

  aircraft.OperatorIDValid = 1;
  strcpy(aircraft.OperatorID.OperatorId, "CHEhackuarium000");
}

void tearDown(void) {}

static void assertWholeAircraft(void) {
  TEST_ASSERT_TRUE(heard.BasicIDValid[0]);
  TEST_ASSERT_EQUAL_STRING("TESTC0123456789AB", heard.BasicID[0].UASID);
  TEST_ASSERT_TRUE(heard.LocationValid);
  TEST_ASSERT_EQUAL_UINT8(ODID_STATUS_AIRBORNE, heard.Location.Status);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 46.5000000, heard.Location.Latitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 6.6000000, heard.Location.Longitude);
  TEST_ASSERT_EQUAL_FLOAT(459.0f, heard.Location.AltitudeGeo);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, heard.Location.Height);
  TEST_ASSERT_TRUE(heard.SelfIDValid);
  TEST_ASSERT_EQUAL_STRING("hackuarium test", heard.SelfID.Desc);
  TEST_ASSERT_TRUE(heard.SystemValid);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 6.6000000,
                            heard.System.OperatorLongitude);
  TEST_ASSERT_EQUAL_FLOAT(449.0f, heard.System.OperatorAltitudeGeo);
  TEST_ASSERT_EQUAL_UINT8(ODID_CLASS_EU_CLASS_0, heard.System.ClassEU);
  TEST_ASSERT_TRUE(heard.OperatorIDValid);
  TEST_ASSERT_EQUAL_STRING("CHEhackuarium000", heard.OperatorID.OperatorId);
}

/* One message per advertisement, filling all 31 bytes, so the five types have
   to take turns. */
static void test_legacy_carries_one_message(void) {
  size_t length = droneIdBuildLegacyAdvertisement(
      &aircraft, ODID_MESSAGETYPE_LOCATION, 0x2A, frame);
  TEST_ASSERT_EQUAL(DRONE_LEGACY_ADVERTISEMENT_LENGTH, length);

  uint8_t payloadLength = 0;
  const uint8_t* payload =
      droneIdFindBluetoothPayload(frame, length, &payloadLength);
  TEST_ASSERT_EQUAL_PTR(&frame[5], payload);
  TEST_ASSERT_EQUAL_UINT8(0x2A, payload[0]);
  TEST_ASSERT_EQUAL_UINT8(1 + ODID_MESSAGE_SIZE, payloadLength);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_LOCATION,
                    droneIdDecodePayload(payload, payloadLength, &heard));
  TEST_ASSERT_TRUE(heard.LocationValid);
  TEST_ASSERT_FALSE(heard.BasicIDValid[0]);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, 46.5000000, heard.Location.Latitude);
}

/* Five turns of the rotation are the whole aircraft. */
static void test_legacy_rotation_is_the_whole_aircraft(void) {
  static const ODID_messagetype_t rotation[] = {
      ODID_MESSAGETYPE_BASIC_ID, ODID_MESSAGETYPE_LOCATION,
      ODID_MESSAGETYPE_SELF_ID, ODID_MESSAGETYPE_SYSTEM,
      ODID_MESSAGETYPE_OPERATOR_ID};
  for (uint8_t i = 0; i < 5; i++) {
    size_t length =
        droneIdBuildLegacyAdvertisement(&aircraft, rotation[i], i, frame);
    uint8_t payloadLength = 0;
    const uint8_t* payload =
        droneIdFindBluetoothPayload(frame, length, &payloadLength);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_EQUAL(rotation[i],
                      droneIdDecodePayload(payload, payloadLength, &heard));
  }
  assertWholeAircraft();
}

/* Five messages in one pack: 6 bytes of service data header and counter, 3 of
   pack header, 125 of messages - well inside one HCI report, so the receiver
   never meets the split it cannot reassemble. */
static void test_extended_carries_a_pack(void) {
  size_t length =
      droneIdBuildExtendedAdvertisement(&aircraft, 7, frame, sizeof(frame));
  TEST_ASSERT_EQUAL(6 + 3 + 5 * ODID_MESSAGE_SIZE, length);
  TEST_ASSERT_EQUAL_UINT8(0x16, frame[1]);

  uint8_t payloadLength = 0;
  const uint8_t* payload =
      droneIdFindBluetoothPayload(frame, length, &payloadLength);
  TEST_ASSERT_EQUAL_PTR(&frame[5], payload);
  TEST_ASSERT_EQUAL_UINT8(7, payload[0]);
  TEST_ASSERT_EQUAL_UINT8(5, payload[3]);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_PACKED,
                    droneIdDecodePayload(payload, payloadLength, &heard));
  assertWholeAircraft();
}

static void test_beacon_carries_a_pack(void) {
  size_t length = droneIdBuildBeaconFrame(&aircraft, address,
                                          "TESTC0123456789AB", 977, 9, frame,
                                          sizeof(frame));
  /* 36 fixed, SSID 2 + 17, rates 3, vendor element 2 + 4 + 1 + 128 */
  TEST_ASSERT_EQUAL(36 + 19 + 3 + 7 + 128, length);
  TEST_ASSERT_EQUAL_UINT8(0x80, frame[0]);
  TEST_ASSERT_EQUAL_MEMORY(address, &frame[10], 6);
  TEST_ASSERT_EQUAL_MEMORY(address, &frame[16], 6);
  TEST_ASSERT_EQUAL_UINT8(977 & 0xFF, frame[32]);
  TEST_ASSERT_EQUAL_UINT8(977 >> 8, frame[33]);

  uint8_t payloadLength = 0;
  const uint8_t* payload =
      droneIdFindBeaconPayload(frame, (uint16_t)length, &payloadLength);
  TEST_ASSERT_NOT_NULL(payload);
  TEST_ASSERT_EQUAL_UINT8(9, payload[0]);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_PACKED,
                    droneIdDecodePayload(payload, payloadLength, &heard));
  assertWholeAircraft();
}

/* A message the record does not hold is not invented, and a buffer too small
   is refused rather than written past. */
static void test_nothing_to_send_builds_nothing(void) {
  aircraft.SelfIDValid = 0;
  TEST_ASSERT_EQUAL(0, droneIdBuildLegacyAdvertisement(
                           &aircraft, ODID_MESSAGETYPE_SELF_ID, 0, frame));
  TEST_ASSERT_EQUAL(0, droneIdBuildLegacyAdvertisement(
                           &aircraft, ODID_MESSAGETYPE_AUTH, 0, frame));
  TEST_ASSERT_EQUAL(0, droneIdBuildExtendedAdvertisement(&aircraft, 0, frame,
                                                         6 + 3 + 50));
  TEST_ASSERT_EQUAL(0, droneIdBuildBeaconFrame(&aircraft, address, "x", 977, 0,
                                               frame, 60));

  odid_initUasData(&aircraft);
  TEST_ASSERT_EQUAL(
      0, droneIdBuildExtendedAdvertisement(&aircraft, 0, frame, sizeof(frame)));
}

/* A kilometre across at 10 m/s: one lap every 100 pi seconds, 314 s. */
#define ORBIT_LATITUDE 46.5000000
#define ORBIT_LONGITUDE 6.6000000
#define ORBIT_LAP (M_PI * 100.0)

static void orbitAt(double seconds, double* latitude, double* longitude,
                    float* course) {
  droneIdOrbitPosition(ORBIT_LATITUDE, ORBIT_LONGITUDE, 500.0, 10.0, seconds,
                       latitude, longitude, course);
}

/* It starts due north flying east, and each quarter lap turns it a quarter
   to the right. */
static void test_orbit_quarters(void) {
  double latitude;
  double longitude;
  float course;

  orbitAt(0, &latitude, &longitude, &course);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, ORBIT_LATITUDE + 500.0 / 111320.0,
                            latitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, ORBIT_LONGITUDE, longitude);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, course);

  orbitAt(ORBIT_LAP / 4, &latitude, &longitude, &course);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, ORBIT_LATITUDE, latitude);
  TEST_ASSERT_TRUE(longitude > ORBIT_LONGITUDE);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, course);

  orbitAt(ORBIT_LAP / 2, &latitude, &longitude, &course);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, ORBIT_LATITUDE - 500.0 / 111320.0,
                            latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 270.0f, course);

  /* 359.99 would be fine; 360 is not a course the encoder accepts */
  orbitAt(ORBIT_LAP * 3 / 4, &latitude, &longitude, &course);
  TEST_ASSERT_TRUE(longitude < ORBIT_LONGITUDE);
  TEST_ASSERT_TRUE(course < 360.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, fmodf(course, 360.0f));
}

/* 500 m from the centre everywhere, 10 m flown per second, and a whole lap
   later it is back where it started - even after days of running. */
static void test_orbit_radius_and_speed(void) {
  double latitude;
  double longitude;
  double previousLatitude;
  double previousLongitude;
  float course;
  orbitAt(0, &previousLatitude, &previousLongitude, &course);
  for (int second = 1; second <= 320; second++) {
    orbitAt(second, &latitude, &longitude, &course);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 500.0,
                              droneIdMetresBetween(ORBIT_LATITUDE,
                                                   ORBIT_LONGITUDE, latitude,
                                                   longitude));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 10.0,
                              droneIdMetresBetween(previousLatitude,
                                                   previousLongitude, latitude,
                                                   longitude));
    previousLatitude = latitude;
    previousLongitude = longitude;
  }

  double startLatitude;
  double startLongitude;
  orbitAt(0, &startLatitude, &startLongitude, &course);
  orbitAt(ORBIT_LAP * 1000, &latitude, &longitude, &course);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, startLatitude, latitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.0000001, startLongitude, longitude);
}

/* The moving aircraft still encodes: speed and course survive the wire. */
static void test_orbit_encodes(void) {
  orbitAt(ORBIT_LAP / 8, &aircraft.Location.Latitude,
          &aircraft.Location.Longitude, &aircraft.Location.Direction);
  aircraft.Location.SpeedHorizontal = 10.0f;
  size_t length = droneIdBuildLegacyAdvertisement(
      &aircraft, ODID_MESSAGETYPE_LOCATION, 0, frame);
  uint8_t payloadLength = 0;
  const uint8_t* payload =
      droneIdFindBluetoothPayload(frame, length, &payloadLength);
  TEST_ASSERT_EQUAL(ODID_MESSAGETYPE_LOCATION,
                    droneIdDecodePayload(payload, payloadLength, &heard));
  TEST_ASSERT_EQUAL_FLOAT(10.0f, heard.Location.SpeedHorizontal);
  /* the wire carries whole degrees */
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 135.0f, heard.Location.Direction);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_legacy_carries_one_message);
  RUN_TEST(test_legacy_rotation_is_the_whole_aircraft);
  RUN_TEST(test_extended_carries_a_pack);
  RUN_TEST(test_beacon_carries_a_pack);
  RUN_TEST(test_nothing_to_send_builds_nothing);
  RUN_TEST(test_orbit_quarters);
  RUN_TEST(test_orbit_radius_and_speed);
  RUN_TEST(test_orbit_encodes);
  return UNITY_END();
}
