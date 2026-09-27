#include <string.h>
#include <unity.h>

#include "loraFrame.h"
#include "mbedtls/ccm.h"

/* The mesh envelope. A byte in the wrong place here does not fail loudly: the
   tag simply stops verifying on the other side, and the node looks deaf. So the
   encoder is checked against frames sealed by a second implementation - the
   expected bytes below come from Node's aes-128-ccm, the decoder a host uses in
   docs/lora-mesh-frame.md - and the decoder against its own failure modes.

   Runs on the host: `pio test -e native`. */

static const uint8_t KEY[LORA_KEY_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                           0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                           0x0c, 0x0d, 0x0e, 0x0f};

/* CMD from 42 to 7, message 1234, setting DA to 1, two hops of budget: the
   worked example of docs/lora-mesh-frame.md, on its first transmission */
static const uint8_t ORIGIN[] = {0x2A, 0x00, 0x04, 0xD2, 0x3B, 0xFE,
                                 0xE7, 0x34, 0x83, 0x23, 0xDD, 0x0D,
                                 0xF6, 0x68, 0x7B, 0x39, 0x62, 0xA4};

/* the same message relayed by node 9, which heard it at -95 dBm and sealed its
   copy with its own counter 5000 */
static const uint8_t RELAYED[] = {0x09, 0x00, 0x13, 0x88, 0xBF, 0xC9, 0x41,
                                  0x63, 0xE8, 0xB3, 0x20, 0xB1, 0x77, 0x1A,
                                  0x45, 0x7C, 0x3F, 0x3A, 0x53, 0x48};

void setUp(void) {}

void tearDown(void) {}

static LoraFrame exampleCommand(void) {
  LoraFrame frame;
  memset(&frame, 0, sizeof(frame));
  frame.transmitter = 42;
  frame.seal = 1234;
  frame.type = LORA_TYPE_CMD;
  frame.budget = 2;
  frame.source = 42;
  frame.destination = 7;
  frame.counter = 1234;
  frame.body[0] = LORA_CMD_SET_PARAMETERS_INT8;
  frame.body[1] = 104;
  frame.body[2] = 1;
  frame.bodyLength = 3;
  return frame;
}

static void test_origin_matches_the_reference(void) {
  LoraFrame frame = exampleCommand();
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length = loraFrameEncode(&frame, KEY, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(sizeof(ORIGIN), length);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(ORIGIN, buffer, sizeof(ORIGIN));
  TEST_ASSERT_EQUAL(LORA_FRAME_OVERHEAD + 3, length);
  TEST_ASSERT_EQUAL(length, loraFrameSize(&frame));
}

static void test_origin_decodes(void) {
  LoraFrame frame;
  TEST_ASSERT_TRUE(loraFrameDecode(ORIGIN, sizeof(ORIGIN), KEY, &frame));
  TEST_ASSERT_EQUAL(42, frame.transmitter);
  TEST_ASSERT_EQUAL(1234, frame.seal);
  TEST_ASSERT_EQUAL(LORA_TYPE_CMD, frame.type);
  TEST_ASSERT_EQUAL(42, frame.source);
  TEST_ASSERT_EQUAL(7, frame.destination);
  TEST_ASSERT_EQUAL(1234, frame.counter);
  TEST_ASSERT_EQUAL(2, frame.budget);
  TEST_ASSERT_EQUAL(0, frame.hops);
  TEST_ASSERT_EQUAL(0, frame.routeLength);
  const uint8_t body[] = {0x01, 0x68, 0x01};
  TEST_ASSERT_EQUAL(sizeof(body), frame.bodyLength);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(body, frame.body, sizeof(body));
}

/* what a relay does: decode, record itself, seal under its own address */
static void test_relay_reseals_with_its_own_nonce(void) {
  LoraFrame frame;
  TEST_ASSERT_TRUE(loraFrameDecode(ORIGIN, sizeof(ORIGIN), KEY, &frame));
  loraFrameRecordRelay(&frame, 9, -95);
  frame.transmitter = 9;
  frame.seal = 5000;
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length = loraFrameEncode(&frame, KEY, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(sizeof(RELAYED), length);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(RELAYED, buffer, sizeof(RELAYED));
}

static void test_relayed_copy_keeps_the_message(void) {
  LoraFrame frame;
  TEST_ASSERT_TRUE(loraFrameDecode(RELAYED, sizeof(RELAYED), KEY, &frame));
  TEST_ASSERT_EQUAL(9, frame.transmitter);
  TEST_ASSERT_EQUAL(5000, frame.seal);
  TEST_ASSERT_EQUAL(42, frame.source);
  TEST_ASSERT_EQUAL(1234, frame.counter);
  TEST_ASSERT_EQUAL(2, frame.budget);
  TEST_ASSERT_EQUAL(1, frame.hops);
  TEST_ASSERT_EQUAL(1, frame.routeLength);
  TEST_ASSERT_EQUAL(9, frame.route[0].address);
  TEST_ASSERT_EQUAL(-95, frame.route[0].rssi);
  TEST_ASSERT_EQUAL(3, frame.bodyLength);
}

/* every byte on the air is bound to the tag, the clear ones included */
static void test_any_changed_byte_is_refused(void) {
  uint8_t copy[sizeof(RELAYED)];
  LoraFrame frame;
  for (uint8_t i = 0; i < sizeof(RELAYED); i++) {
    memcpy(copy, RELAYED, sizeof(RELAYED));
    copy[i] ^= 0x01;
    TEST_ASSERT_FALSE_MESSAGE(loraFrameDecode(copy, sizeof(copy), KEY, &frame),
                              "a flipped bit still verified");
  }
}

static void test_wrong_key_is_refused(void) {
  uint8_t other[LORA_KEY_SIZE];
  memcpy(other, KEY, sizeof(other));
  other[15] ^= 0x80;
  LoraFrame frame;
  TEST_ASSERT_FALSE(loraFrameDecode(ORIGIN, sizeof(ORIGIN), other, &frame));
}

static void test_short_input_is_refused(void) {
  LoraFrame frame;
  for (uint8_t length = 0; length < sizeof(ORIGIN); length++) {
    TEST_ASSERT_FALSE(loraFrameDecode(ORIGIN, length, KEY, &frame));
  }
}

static void test_wide_counters_round_trip(void) {
  LoraFrame frame = exampleCommand();
  frame.counter = LORA_COUNTER_SHORT_MAX + 1;
  frame.seal = LORA_COUNTER_MAX;
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length = loraFrameEncode(&frame, KEY, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(LORA_FRAME_OVERHEAD + 3 + 2, length);
  TEST_ASSERT_EQUAL(length, loraFrameSize(&frame));
  /* the seal's width is readable without the key */
  TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[1]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[4]);

  LoraFrame decoded;
  TEST_ASSERT_TRUE(loraFrameDecode(buffer, length, KEY, &decoded));
  TEST_ASSERT_EQUAL_UINT32(LORA_COUNTER_SHORT_MAX + 1, decoded.counter);
  TEST_ASSERT_EQUAL_UINT32(LORA_COUNTER_MAX, decoded.seal);
}

static void test_counter_past_the_range_is_not_encoded(void) {
  LoraFrame frame = exampleCommand();
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  frame.seal = LORA_COUNTER_MAX + 1;
  TEST_ASSERT_EQUAL(0, loraFrameEncode(&frame, KEY, buffer, sizeof(buffer)));
  frame = exampleCommand();
  frame.counter = LORA_COUNTER_MAX + 1;
  TEST_ASSERT_EQUAL(0, loraFrameEncode(&frame, KEY, buffer, sizeof(buffer)));
}

static void test_largest_frame_fits_the_buffer(void) {
  LoraFrame frame = exampleCommand();
  frame.counter = LORA_COUNTER_MAX;
  frame.seal = LORA_COUNTER_MAX;
  memset(frame.body, 0xA5, sizeof(frame.body));
  frame.bodyLength = LORA_MAX_BODY_SIZE;
  for (uint8_t i = 0; i < LORA_ROUTE_MAX; i++) {
    loraFrameRecordRelay(&frame, (uint8_t)(10 + i), -100);
  }
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length = loraFrameEncode(&frame, KEY, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL(LORA_MAX_FRAME_SIZE, length);
  TEST_ASSERT_EQUAL(73, length);
  TEST_ASSERT_EQUAL(length, loraFrameSize(&frame));
  TEST_ASSERT_EQUAL(0, loraFrameEncode(&frame, KEY, buffer, length - 1));

  LoraFrame decoded;
  TEST_ASSERT_TRUE(loraFrameDecode(buffer, length, KEY, &decoded));
  TEST_ASSERT_EQUAL(LORA_MAX_BODY_SIZE, decoded.bodyLength);
  TEST_ASSERT_EQUAL(LORA_ROUTE_MAX, decoded.routeLength);
  TEST_ASSERT_EQUAL(13, decoded.route[3].address);
}

static void test_route_stops_recording_past_the_table(void) {
  LoraFrame frame = exampleCommand();
  for (uint8_t i = 0; i < LORA_ROUTE_MAX + 1; i++) {
    loraFrameRecordRelay(&frame, (uint8_t)(10 + i), -300);
  }
  TEST_ASSERT_EQUAL(LORA_ROUTE_MAX + 1, frame.hops);
  TEST_ASSERT_EQUAL(LORA_ROUTE_MAX, frame.routeLength);
  TEST_ASSERT_EQUAL(13, frame.route[3].address);
  /* clamped to the int8 an entry holds */
  TEST_ASSERT_EQUAL(-128, frame.route[0].rssi);

  frame.hops = LORA_HOPS_MAX;
  loraFrameRecordRelay(&frame, 99, -90);
  TEST_ASSERT_EQUAL(LORA_HOPS_MAX, frame.hops);

  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length = loraFrameEncode(&frame, KEY, buffer, sizeof(buffer));
  LoraFrame decoded;
  TEST_ASSERT_TRUE(loraFrameDecode(buffer, length, KEY, &decoded));
  TEST_ASSERT_EQUAL(LORA_HOPS_MAX, decoded.hops);
  TEST_ASSERT_EQUAL(LORA_ROUTE_MAX, decoded.routeLength);
}

static void test_inconsistent_route_is_not_encoded(void) {
  LoraFrame frame = exampleCommand();
  frame.hops = 2;
  frame.routeLength = 1;
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  TEST_ASSERT_EQUAL(0, loraFrameSize(&frame));
  TEST_ASSERT_EQUAL(0, loraFrameEncode(&frame, KEY, buffer, sizeof(buffer)));
}

/* A frame of another version is refused even when its tag verifies - the
   plaintext is resealed here by hand, with the nonce the document describes */
static void test_other_version_is_refused(void) {
  uint8_t plain[] = {0x40, 0x2A, 0x07, 0x00, 0x04, 0xD2,
                     0x01, 0x68, 0x01, 0x20};
  uint8_t nonce[LORA_NONCE_SIZE] = {0x01, 0x2A, 0x00, 0x00, 0x04, 0xD2};
  uint8_t frame[4 + sizeof(plain) + LORA_MIC_SIZE] = {0x2A, 0x00, 0x04, 0xD2};
  mbedtls_ccm_context context;
  mbedtls_ccm_init(&context);
  TEST_ASSERT_EQUAL(0, mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, KEY,
                                          LORA_KEY_SIZE * 8));
  TEST_ASSERT_EQUAL(
      0, mbedtls_ccm_encrypt_and_tag(&context, sizeof(plain), nonce,
                                     sizeof(nonce), frame, 4, plain, frame + 4,
                                     frame + 4 + sizeof(plain), LORA_MIC_SIZE));
  mbedtls_ccm_free(&context);

  LoraFrame decoded;
  TEST_ASSERT_FALSE(loraFrameDecode(frame, sizeof(frame), KEY, &decoded));
  /* and the same bytes with the version bit set are the reference frame */
  plain[0] = 0xC0;
  mbedtls_ccm_init(&context);
  mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, KEY, LORA_KEY_SIZE * 8);
  mbedtls_ccm_encrypt_and_tag(&context, sizeof(plain), nonce, sizeof(nonce),
                              frame, 4, plain, frame + 4,
                              frame + 4 + sizeof(plain), LORA_MIC_SIZE);
  mbedtls_ccm_free(&context);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(ORIGIN, frame, sizeof(ORIGIN));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_origin_matches_the_reference);
  RUN_TEST(test_origin_decodes);
  RUN_TEST(test_relay_reseals_with_its_own_nonce);
  RUN_TEST(test_relayed_copy_keeps_the_message);
  RUN_TEST(test_any_changed_byte_is_refused);
  RUN_TEST(test_wrong_key_is_refused);
  RUN_TEST(test_short_input_is_refused);
  RUN_TEST(test_wide_counters_round_trip);
  RUN_TEST(test_counter_past_the_range_is_not_encoded);
  RUN_TEST(test_largest_frame_fits_the_buffer);
  RUN_TEST(test_route_stops_recording_past_the_table);
  RUN_TEST(test_inconsistent_route_is_not_encoded);
  RUN_TEST(test_other_version_is_refused);
  return UNITY_END();
}
