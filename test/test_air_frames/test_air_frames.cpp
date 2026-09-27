#include <string.h>
#include <unity.h>

#include "airFrames.h"
#include "airSchedule.h"

/* The air traffic receiver's framing and its timetable.

   No frame here was heard on the air yet - the board has not been in the
   field - so the frames are built by encoding, and what makes them worth
   testing against is that the checks come from outside this code: the CRC-24
   against the Mode S message every ADS-B tutorial decodes, the CRC-16 against
   the catalogued check value, and the sync words against the bytes SoftRF, the
   one open implementation that talks to all of them, puts on the air. The
   first real capture should be frozen here as fixtures, as the drone watcher's
   were.

   Runs on the host: `pio test -e native`. */

void setUp(void) {}

void tearDown(void) {}

/* A transmitter's half of Manchester, IEEE convention, which the receiver
   never needs - so it lives here and not beside the decoder. */
static void manchesterEncode(const uint8_t* data, size_t count,
                             uint8_t* chips) {
  for (size_t i = 0; i < count; i++) {
    uint16_t word = 0;
    for (int8_t bit = 7; bit >= 0; bit--) {
      word = (uint16_t)(word << 2) | (((data[i] >> bit) & 1) ? 0x1 : 0x2);
    }
    chips[2 * i] = (uint8_t)(word >> 8);
    chips[2 * i + 1] = (uint8_t)word;
  }
}

/* What the radio hands over after the M-band sync word: the rest of the frame
   as chips, then noise to the end of the fixed capture length. */
static size_t mbandCapture(const uint8_t* frame, size_t length,
                           uint8_t* chips) {
  memset(chips, 0x5A, AIR_CAPTURE_MAX);
  manchesterEncode(frame, length, chips);
  return AIR_CAPTURE_MAX;
}

static size_t flarmFrame(uint8_t* frame) {
  static const uint8_t syncTail[] = {0x31, 0xFA, 0xB6};
  memcpy(frame, syncTail, sizeof(syncTail));
  for (uint8_t i = 0; i < 24; i++) {
    frame[3 + i] = (uint8_t)(0x10 + 7 * i);
  }
  uint16_t crc = airCrc16Ccitt(0xFFFF, frame, 27);
  frame[27] = (uint8_t)(crc >> 8);
  frame[28] = (uint8_t)crc;
  return 29;
}

static size_t adslFrame(uint8_t* frame) {
  frame[0] = 0x72;
  frame[1] = 0x4B;
  frame[2] = 24;
  for (uint8_t i = 0; i < 21; i++) {
    frame[3 + i] = (uint8_t)(0xA0 ^ (13 * i));
  }
  uint32_t crc = airCrc24(frame + 3, 21);
  frame[24] = (uint8_t)(crc >> 16);
  frame[25] = (uint8_t)(crc >> 8);
  frame[26] = (uint8_t)crc;
  return 27;
}

static void test_manchester_of_the_three_sync_words(void) {
  static const uint8_t flarm[] = {0x55, 0x99, 0xA5, 0xA9,
                                  0x55, 0x66, 0x65, 0x96};
  static const uint8_t adsl[] = {0x55, 0x99, 0x95, 0xA6,
                                 0x9A, 0x65, 0xA9, 0x6A};
  static const uint8_t ogn[] = {0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A};
  uint8_t decoded[4];

  TEST_ASSERT_EQUAL_UINT8(0, airManchesterDecode(flarm, decoded, 4));
  static const uint8_t flarmWord[] = {0xF5, 0x31, 0xFA, 0xB6};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(flarmWord, decoded, 4);

  TEST_ASSERT_EQUAL_UINT8(0, airManchesterDecode(adsl, decoded, 4));
  static const uint8_t adslWord[] = {0xF5, 0x72, 0x4B, 0x18};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(adslWord, decoded, 4);

  TEST_ASSERT_EQUAL_UINT8(0, airManchesterDecode(ogn, decoded, 4));
  static const uint8_t ognWord[] = {0x0A, 0xF3, 0x65, 0x6C};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(ognWord, decoded, 4);

  /* and the shared sync word the radio matches is the Manchester of 0xF5 */
  const AirListenSetting* mband = airListenSetting(AIR_LISTEN_MBAND_LOW);
  TEST_ASSERT_EQUAL(2, mband->syncWordLength);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(flarm, mband->syncWord, 2);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(adsl, mband->syncWord, 2);
}

static void test_manchester_counts_violations(void) {
  static const uint8_t chips[] = {0x00, 0xFF, 0x59, 0x99};
  uint8_t decoded[2];
  /* 00 and 11 are never sent: the first byte is made of eight of them, the
     second decodes cleanly */
  TEST_ASSERT_EQUAL_UINT8(8, airManchesterDecode(chips, decoded, 2));
  TEST_ASSERT_EQUAL_HEX8(0x00, decoded[0]);
  TEST_ASSERT_EQUAL_HEX8(0xD5, decoded[1]);
}

static void test_crc16_check_value(void) {
  const uint8_t text[] = "123456789";
  TEST_ASSERT_EQUAL_HEX16(0x29B1, airCrc16Ccitt(0xFFFF, text, 9));
}

/* The spec says ADS-L's CRC is Mode S's, and this is the parity of the Mode S
   message every ADS-B tutorial starts from: 8D4840D6202CC371C32CE0 576098. */
static void test_crc24_is_the_mode_s_parity(void) {
  static const uint8_t message[] = {0x8D, 0x48, 0x40, 0xD6, 0x20, 0x2C,
                                    0xC3, 0x71, 0xC3, 0x2C, 0xE0};
  TEST_ASSERT_EQUAL_HEX32(0x576098, airCrc24(message, sizeof(message)));
}

static void test_flarm_frame(void) {
  uint8_t frame[29];
  uint8_t chips[AIR_CAPTURE_MAX];
  size_t length = mbandCapture(frame, flarmFrame(frame), chips);

  AirFrame parsed;
  TEST_ASSERT_TRUE(airFrameParse(AIR_LISTEN_MBAND_LOW, chips, length, &parsed));
  TEST_ASSERT_EQUAL(AIR_FLARM, parsed.protocol);
  TEST_ASSERT_EQUAL_STRING("flarm", airProtocolName(parsed.protocol));
  TEST_ASSERT_EQUAL_UINT8(26, parsed.length);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(frame + 3, parsed.bytes, 26);
  TEST_ASSERT_EQUAL_INT8(1, parsed.check);
  TEST_ASSERT_EQUAL_UINT8(0, parsed.violations);

  /* one payload bit flipped - 01 becomes 10 - is a failed CRC, not a guess */
  chips[20] ^= 0xC0;
  TEST_ASSERT_TRUE(
      airFrameParse(AIR_LISTEN_MBAND_HIGH, chips, length, &parsed));
  TEST_ASSERT_EQUAL(AIR_FLARM, parsed.protocol);
  TEST_ASSERT_EQUAL_INT8(0, parsed.check);
  TEST_ASSERT_EQUAL_UINT8(0, parsed.violations);

  /* and a chip pair that Manchester never sends is counted */
  chips[20] = (uint8_t)((chips[20] & 0x3F) | 0xC0);
  TEST_ASSERT_TRUE(airFrameParse(AIR_LISTEN_MBAND_LOW, chips, length, &parsed));
  TEST_ASSERT_EQUAL_UINT8(1, parsed.violations);
}

static void test_adsl_frame_on_the_m_band(void) {
  uint8_t frame[27];
  uint8_t chips[AIR_CAPTURE_MAX];
  size_t length = mbandCapture(frame, adslFrame(frame), chips);

  AirFrame parsed;
  TEST_ASSERT_TRUE(airFrameParse(AIR_LISTEN_MBAND_LOW, chips, length, &parsed));
  TEST_ASSERT_EQUAL(AIR_ADSL, parsed.protocol);
  TEST_ASSERT_EQUAL_UINT8(25, parsed.length);
  TEST_ASSERT_EQUAL_HEX8(24, parsed.bytes[0]);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(frame + 2, parsed.bytes, 25);
  TEST_ASSERT_EQUAL_INT8(1, parsed.check);

  /* a length that runs past what was captured is not a frame */
  frame[2] = 40;
  length = mbandCapture(frame, 27, chips);
  TEST_ASSERT_FALSE(
      airFrameParse(AIR_LISTEN_MBAND_LOW, chips, length, &parsed));
}

/* The 16-chip sync word matches noise; what follows it then is neither
   protocol's, and the capture is dropped rather than reported. */
static void test_noise_after_the_sync_word_is_dropped(void) {
  static const uint8_t other[] = {0x31, 0xFA, 0xB7, 0x00, 0x11, 0x22};
  uint8_t chips[AIR_CAPTURE_MAX];
  size_t length = mbandCapture(other, sizeof(other), chips);
  AirFrame parsed;
  TEST_ASSERT_FALSE(
      airFrameParse(AIR_LISTEN_MBAND_LOW, chips, length, &parsed));

  /* a clean FLARM head with a broken chip in it says nothing either */
  uint8_t frame[29];
  length = mbandCapture(frame, flarmFrame(frame), chips);
  chips[1] = 0x98;
  TEST_ASSERT_FALSE(
      airFrameParse(AIR_LISTEN_MBAND_LOW, chips, length, &parsed));

  /* and a capture too short to hold a frame is not read past its end */
  length = mbandCapture(frame, flarmFrame(frame), chips);
  TEST_ASSERT_FALSE(airFrameParse(AIR_LISTEN_MBAND_LOW, chips, 40, &parsed));
}

static void test_ogn_frame_is_kept_unchecked(void) {
  uint8_t frame[26];
  for (uint8_t i = 0; i < 26; i++) {
    frame[i] = (uint8_t)(3 * i + 1);
  }
  uint8_t chips[52];
  manchesterEncode(frame, 26, chips);

  AirFrame parsed;
  TEST_ASSERT_TRUE(airFrameParse(AIR_LISTEN_OGN_HIGH, chips, 52, &parsed));
  TEST_ASSERT_EQUAL(AIR_OGN, parsed.protocol);
  TEST_ASSERT_EQUAL_UINT8(26, parsed.length);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(frame, parsed.bytes, 26);
  TEST_ASSERT_EQUAL_INT8(-1, parsed.check);
  TEST_ASSERT_EQUAL_UINT8(0, parsed.violations);
}

static void test_adsl_frame_on_the_o_band(void) {
  uint8_t mband[27];
  adslFrame(mband);
  /* the O-band is not Manchester coded: length, then the packet, then
     whatever the fixed capture length reads past its end */
  uint8_t raw[32];
  memset(raw, 0xE7, sizeof(raw));
  memcpy(raw, mband + 2, 25);

  AirFrame parsed;
  TEST_ASSERT_TRUE(airFrameParse(AIR_LISTEN_OBAND, raw, sizeof(raw), &parsed));
  TEST_ASSERT_EQUAL(AIR_ADSL, parsed.protocol);
  TEST_ASSERT_EQUAL_UINT8(25, parsed.length);
  TEST_ASSERT_EQUAL_INT8(1, parsed.check);

  raw[10] ^= 0x01;
  TEST_ASSERT_TRUE(airFrameParse(AIR_LISTEN_OBAND, raw, sizeof(raw), &parsed));
  TEST_ASSERT_EQUAL_INT8(0, parsed.check);

  raw[0] = 3;
  TEST_ASSERT_FALSE(airFrameParse(AIR_LISTEN_OBAND, raw, sizeof(raw), &parsed));
}

static void test_fanet_payload_is_copied(void) {
  static const uint8_t payload[] = {0x41, 0x07, 0x12, 0x34, 0x56, 0x78};
  AirFrame parsed;
  TEST_ASSERT_TRUE(
      airFrameParse(AIR_LISTEN_FANET, payload, sizeof(payload), &parsed));
  TEST_ASSERT_EQUAL(AIR_FANET, parsed.protocol);
  TEST_ASSERT_EQUAL_UINT8(sizeof(payload), parsed.length);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, parsed.bytes, sizeof(payload));
  TEST_ASSERT_EQUAL_INT8(-1, parsed.check);
  TEST_ASSERT_FALSE(airFrameParse(AIR_LISTEN_FANET, payload, 0, &parsed));
}

#define DEFAULT_MASK (AIR_MASK_MBAND | AIR_MASK_OGN | AIR_MASK_FANET)

/* Second 10 is a FLARM cycle, second 11 the OGN one (11 % 4 == 3). */
static void test_timetable_follows_flarm_then_ogn(void) {
  TEST_ASSERT_EQUAL(AIR_LISTEN_FANET, airListenAtUtc(DEFAULT_MASK, 10, 200));
  TEST_ASSERT_EQUAL(AIR_LISTEN_FANET, airListenAtUtc(DEFAULT_MASK, 10, 399));
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_LOW,
                    airListenAtUtc(DEFAULT_MASK, 10, 400));
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_LOW,
                    airListenAtUtc(DEFAULT_MASK, 10, 799));
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_HIGH,
                    airListenAtUtc(DEFAULT_MASK, 10, 800));
  /* slot 1 of second 10 runs into second 11 */
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_HIGH,
                    airListenAtUtc(DEFAULT_MASK, 11, 199));

  TEST_ASSERT_EQUAL(AIR_LISTEN_FANET, airListenAtUtc(DEFAULT_MASK, 11, 300));
  TEST_ASSERT_EQUAL(AIR_LISTEN_OGN_HIGH, airListenAtUtc(DEFAULT_MASK, 11, 500));
  TEST_ASSERT_EQUAL(AIR_LISTEN_OGN_LOW, airListenAtUtc(DEFAULT_MASK, 11, 900));
  TEST_ASSERT_EQUAL(AIR_LISTEN_OGN_LOW, airListenAtUtc(DEFAULT_MASK, 12, 100));
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_LOW,
                    airListenAtUtc(DEFAULT_MASK, 12, 600));
}

static void test_timetable_with_one_protocol(void) {
  /* OGN alone follows OGN every second */
  TEST_ASSERT_EQUAL(AIR_LISTEN_OGN_HIGH, airListenAtUtc(AIR_MASK_OGN, 10, 500));
  TEST_ASSERT_EQUAL(AIR_LISTEN_OGN_LOW, airListenAtUtc(AIR_MASK_OGN, 10, 900));
  /* nothing in the gap to hear: the M-band stays on, for unslotted ADS-L */
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_LOW,
                    airListenAtUtc(AIR_MASK_MBAND, 10, 300));
  /* FANET alone has the whole second */
  TEST_ASSERT_EQUAL(AIR_LISTEN_FANET, airListenAtUtc(AIR_MASK_FANET, 10, 500));
  TEST_ASSERT_EQUAL(AIR_LISTEN_FANET, airListenAtUtc(AIR_MASK_FANET, 10, 900));
  /* the O-band takes one gap in four from FANET */
  uint8_t gapBoth = AIR_MASK_FANET | AIR_MASK_OBAND;
  TEST_ASSERT_EQUAL(AIR_LISTEN_FANET, airListenAtUtc(gapBoth, 10, 300));
  TEST_ASSERT_EQUAL(AIR_LISTEN_OBAND, airListenAtUtc(gapBoth, 11, 300));
  TEST_ASSERT_EQUAL(AIR_LISTEN_NONE, airListenAtUtc(0, 10, 500));
}

/* Midnight: 86400 is a multiple of the share turn, so the turn does not skip
   when the second of day wraps, and the cycle before second 0 is 86399's. */
static void test_timetable_across_midnight(void) {
  TEST_ASSERT_EQUAL(AIR_LISTEN_OGN_LOW, airListenAtUtc(DEFAULT_MASK, 0, 100));
  TEST_ASSERT_EQUAL(AIR_LISTEN_OGN_LOW,
                    airListenAtUtc(DEFAULT_MASK, 86399, 900));
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_LOW, airListenAtUtc(DEFAULT_MASK, 0, 500));
}

static void test_rotation_without_a_clock(void) {
  uint8_t mask = AIR_MASK_MBAND | AIR_MASK_FANET;
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_LOW, airListenRotating(mask, 0));
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_HIGH, airListenRotating(mask, 1));
  TEST_ASSERT_EQUAL(AIR_LISTEN_FANET, airListenRotating(mask, 2));
  TEST_ASSERT_EQUAL(AIR_LISTEN_MBAND_LOW, airListenRotating(mask, 3));
  TEST_ASSERT_EQUAL(AIR_LISTEN_OBAND, airListenRotating(AIR_MASK_ALL, 5));
  TEST_ASSERT_EQUAL(AIR_LISTEN_NONE, airListenRotating(0, 7));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_manchester_of_the_three_sync_words);
  RUN_TEST(test_manchester_counts_violations);
  RUN_TEST(test_crc16_check_value);
  RUN_TEST(test_crc24_is_the_mode_s_parity);
  RUN_TEST(test_flarm_frame);
  RUN_TEST(test_adsl_frame_on_the_m_band);
  RUN_TEST(test_noise_after_the_sync_word_is_dropped);
  RUN_TEST(test_ogn_frame_is_kept_unchecked);
  RUN_TEST(test_adsl_frame_on_the_o_band);
  RUN_TEST(test_fanet_payload_is_copied);
  RUN_TEST(test_timetable_follows_flarm_then_ogn);
  RUN_TEST(test_timetable_with_one_protocol);
  RUN_TEST(test_timetable_across_midnight);
  RUN_TEST(test_rotation_without_a_clock);
  return UNITY_END();
}
