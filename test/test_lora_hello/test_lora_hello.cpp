#include <string.h>
#include <unity.h>

#include "loraHello.h"

/* The HELLO body and the location line. The host decodes the first from the
   hex a bridge prints and parses the second out of (al) and (ai), and neither
   side can tell a byte or a digit in the wrong place from a node standing
   somewhere else - so both are checked against exact bytes and exact strings.

   Runs on the host: `pio test -e native`. */

void setUp(void) {}

void tearDown(void) {}

static LoraHello placed(int32_t latitude, int32_t longitude) {
  LoraHello hello;
  memset(&hello, 0, sizeof(hello));
  hello.located = true;
  hello.latitude = latitude;
  hello.longitude = longitude;
  return hello;
}

static void assertLine(const char* expected, const LoraHello* hello) {
  char line[64];
  size_t length = loraHelloFormatLocation(hello, line, sizeof(line));
  TEST_ASSERT_EQUAL_STRING(expected, line);
  TEST_ASSERT_EQUAL(strlen(expected), length);
}

static void test_hello_without_position(void) {
  LoraHello hello;
  memset(&hello, 0, sizeof(hello));
  uint8_t body[LORA_HELLO_MAX_SIZE];
  TEST_ASSERT_EQUAL(1, loraHelloEncode(&hello, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(0x00, body[0]);

  hello.droneWatcher = true;
  TEST_ASSERT_EQUAL(1, loraHelloEncode(&hello, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(0x04, body[0]);

  /* the GPS bit qualifies a position, so without one it is never set */
  hello.gps = true;
  TEST_ASSERT_EQUAL(1, loraHelloEncode(&hello, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(0x04, body[0]);
}

static void test_hello_from_a_repeater(void) {
  LoraHello hello;
  memset(&hello, 0, sizeof(hello));
  hello.repeater = true;
  uint8_t body[LORA_HELLO_MAX_SIZE];
  TEST_ASSERT_EQUAL(1, loraHelloEncode(&hello, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(0x08, body[0]);

  /* a watcher that also relays, standing on its fix */
  hello = placed(46519100, 6566800);
  hello.gps = true;
  hello.droneWatcher = true;
  hello.repeater = true;
  TEST_ASSERT_EQUAL(9, loraHelloEncode(&hello, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8(0x0F, body[0]);
}

static void test_hello_placed_by_hand_on_a_watcher(void) {
  LoraHello hello = placed(46519100, 6566800);
  hello.droneWatcher = true;
  uint8_t body[LORA_HELLO_MAX_SIZE];
  const uint8_t expected[] = {0x05, 0x3C, 0xD3, 0xC5, 0x02,
                              0x90, 0x33, 0x64, 0x00};
  TEST_ASSERT_EQUAL(sizeof(expected),
                    loraHelloEncode(&hello, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, body, sizeof(expected));
}

static void test_hello_with_a_fix(void) {
  LoraHello hello = placed(-33856784, 151215297);
  hello.gps = true;
  uint8_t body[LORA_HELLO_MAX_SIZE];
  const uint8_t expected[] = {0x03, 0xF0, 0x62, 0xFB, 0xFD,
                              0xC1, 0x5C, 0x03, 0x09};
  TEST_ASSERT_EQUAL(sizeof(expected),
                    loraHelloEncode(&hello, body, sizeof(body)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, body, sizeof(expected));
}

static void test_hello_that_does_not_fit(void) {
  LoraHello hello = placed(46519100, 6566800);
  uint8_t body[LORA_HELLO_MAX_SIZE];
  TEST_ASSERT_EQUAL(0, loraHelloEncode(&hello, body, LORA_HELLO_MAX_SIZE - 1));
  hello.located = false;
  TEST_ASSERT_EQUAL(0, loraHelloEncode(&hello, body, 0));
}

static void test_location_lines(void) {
  LoraHello hello = placed(46519100, 6566800);
  assertLine("Location: 46.519100,6.566800 (fixed)", &hello);
  hello.gps = true;
  assertLine("Location: 46.519100,6.566800 (gps)", &hello);

  hello.located = false;
  assertLine("Location: not set", &hello);
}

static void test_location_line_signs_and_digits(void) {
  /* the sign is its own field, since the integer part of -0.5 is 0 */
  LoraHello hello = placed(-500000, -1);
  assertLine("Location: -0.500000,-0.000001 (fixed)", &hello);

  /* integer arithmetic, so nothing rounds up into the next degree */
  hello = placed(999999, -179999999);
  assertLine("Location: 0.999999,-179.999999 (fixed)", &hello);

  hello = placed(0, 7);
  assertLine("Location: 0.000000,0.000007 (fixed)", &hello);
}

static void test_location_line_fits_one_console_reply(void) {
  LoraHello hello = placed(-90000000, -180000000);
  char line[64];
  size_t length = loraHelloFormatLocation(&hello, line, sizeof(line));
  TEST_ASSERT_EQUAL_STRING("Location: -90.000000,-180.000000 (fixed)", line);
  TEST_ASSERT_EQUAL(LORA_HELLO_LOCATION_MAX_LENGTH, length);

  hello = placed(-89123456, -179123456);
  TEST_ASSERT_EQUAL(LORA_HELLO_LOCATION_MAX_LENGTH,
                    loraHelloFormatLocation(&hello, line, sizeof(line)));
}

static void test_parse_accepts(void) {
  int32_t latitude = 1;
  int32_t longitude = 1;
  TEST_ASSERT_TRUE(loraHelloParseLocation("46.5191,6.5668", &latitude,
                                          &longitude));
  TEST_ASSERT_EQUAL_INT32(46519100, latitude);
  TEST_ASSERT_EQUAL_INT32(6566800, longitude);

  TEST_ASSERT_TRUE(loraHelloParseLocation("-33.856784,151.215297", &latitude,
                                          &longitude));
  TEST_ASSERT_EQUAL_INT32(-33856784, latitude);
  TEST_ASSERT_EQUAL_INT32(151215297, longitude);

  TEST_ASSERT_TRUE(loraHelloParseLocation("46,-6", &latitude, &longitude));
  TEST_ASSERT_EQUAL_INT32(46000000, latitude);
  TEST_ASSERT_EQUAL_INT32(-6000000, longitude);

  TEST_ASSERT_TRUE(loraHelloParseLocation("-90,180", &latitude, &longitude));
  TEST_ASSERT_EQUAL_INT32(-90000000, latitude);
  TEST_ASSERT_EQUAL_INT32(180000000, longitude);

  /* 0,0 parses; it is the caller that reads it as "forget" */
  TEST_ASSERT_TRUE(loraHelloParseLocation("0,0", &latitude, &longitude));
  TEST_ASSERT_EQUAL_INT32(0, latitude);
  TEST_ASSERT_EQUAL_INT32(0, longitude);
}

static void test_parse_accepts_spaces_around_the_comma(void) {
  const char* spaced[] = {"46.5191, 6.5668", "46.5191 ,6.5668",
                          "46.5191 , 6.5668", "46.5191,   6.5668"};
  for (size_t i = 0; i < sizeof(spaced) / sizeof(spaced[0]); i++) {
    int32_t latitude = 1;
    int32_t longitude = 1;
    TEST_ASSERT_TRUE_MESSAGE(
        loraHelloParseLocation(spaced[i], &latitude, &longitude), spaced[i]);
    TEST_ASSERT_EQUAL_INT32(46519100, latitude);
    TEST_ASSERT_EQUAL_INT32(6566800, longitude);
  }

  int32_t latitude = 1;
  int32_t longitude = 1;
  TEST_ASSERT_TRUE(loraHelloParseLocation("-33.856784, -151.215297", &latitude,
                                          &longitude));
  TEST_ASSERT_EQUAL_INT32(-33856784, latitude);
  TEST_ASSERT_EQUAL_INT32(-151215297, longitude);
}

static void test_parse_rounds_to_the_microdegree(void) {
  int32_t latitude;
  int32_t longitude;
  TEST_ASSERT_TRUE(loraHelloParseLocation("46.51914449,-6.56680051",
                                          &latitude, &longitude));
  TEST_ASSERT_EQUAL_INT32(46519144, latitude);
  TEST_ASSERT_EQUAL_INT32(-6566801, longitude);
}

static void test_parse_rejects(void) {
  const char* refused[] = {
      "",
      "46.5191",
      "46.5191,",
      ",6.5668",
      "46.5191;6.5668",
      "46.5191 6.5668",
      " 46.5191,6.5668",
      "46.5191,6.5668 ",
      "46.5191,\t6.5668",
      "46.5191, ,6.5668",
      "- 46.5191,6.5668",
      "46.5191,- 6.5668",
      "+46.5,6.5",
      "46.5191,6.5668x",
      "46,6,7",
      "46.5.1,6",
      "--46,6",
      "-,6",
      ".,6",
      "nan,0",
      "inf,0",
      "4e1,6",
      "0x2E,6",
      "90.000001,0",
      "0,-180.000001",
  };
  for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
    int32_t latitude = 11;
    int32_t longitude = 22;
    TEST_ASSERT_FALSE_MESSAGE(
        loraHelloParseLocation(refused[i], &latitude, &longitude), refused[i]);
    TEST_ASSERT_EQUAL_INT32(11, latitude);
    TEST_ASSERT_EQUAL_INT32(22, longitude);
  }
}

static void test_valid_position(void) {
  TEST_ASSERT_TRUE(loraHelloIsValidPosition(90000000, -180000000));
  TEST_ASSERT_FALSE(loraHelloIsValidPosition(90000001, 0));
  TEST_ASSERT_FALSE(loraHelloIsValidPosition(0, -180000001));
  /* two ERROR_VALUE halves read back as one int32 */
  TEST_ASSERT_FALSE(loraHelloIsValidPosition((int32_t)0x80008000, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hello_without_position);
  RUN_TEST(test_hello_from_a_repeater);
  RUN_TEST(test_hello_placed_by_hand_on_a_watcher);
  RUN_TEST(test_hello_with_a_fix);
  RUN_TEST(test_hello_that_does_not_fit);
  RUN_TEST(test_location_lines);
  RUN_TEST(test_location_line_signs_and_digits);
  RUN_TEST(test_location_line_fits_one_console_reply);
  RUN_TEST(test_parse_accepts);
  RUN_TEST(test_parse_accepts_spaces_around_the_comma);
  RUN_TEST(test_parse_rounds_to_the_microdegree);
  RUN_TEST(test_parse_rejects);
  RUN_TEST(test_valid_position);
  return UNITY_END();
}
