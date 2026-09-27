#include "airFrames.h"

#include <string.h>

/* The rest of FLARM's sync word after the shared 0xF5, and ADS-L's own sync
   word (C.2.4) - see airProtocols.cpp for why the radio stops at 0xF5. */
static const uint8_t flarmSyncTail[] = {0x31, 0xFA, 0xB6};
static const uint8_t adslSyncWord[] = {0x72, 0x4B};

#define FLARM_PAYLOAD_LENGTH 24
#define FLARM_CRC_LENGTH 2
#define FLARM_FRAME_LENGTH \
  (sizeof(flarmSyncTail) + FLARM_PAYLOAD_LENGTH + FLARM_CRC_LENGTH)
#define ADSL_CRC_LENGTH 3
/* The network header byte and the CRC: nothing shorter is a packet (E.1). */
#define ADSL_MIN_LENGTH (1 + ADSL_CRC_LENGTH)
#define OGN_FRAME_LENGTH 26
#define MBAND_MAX_DECODED (AIR_CAPTURE_MAX / 2)

uint8_t airManchesterDecode(const uint8_t* chips, uint8_t* out, size_t count) {
  uint16_t violations = 0;
  for (size_t i = 0; i < count; i++) {
    uint8_t value = 0;
    for (uint8_t half = 0; half < 2; half++) {
      uint8_t chipByte = chips[2 * i + half];
      for (int8_t shift = 6; shift >= 0; shift -= 2) {
        uint8_t pair = (chipByte >> shift) & 0x03;
        value <<= 1;
        if (pair == 0x01) {
          value |= 1;
        } else if (pair != 0x02) {
          violations++;
        }
      }
    }
    out[i] = value;
  }
  return violations > 255 ? 255 : (uint8_t)violations;
}

uint16_t airCrc16Ccitt(uint16_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                           : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static uint32_t crc24Pass(uint32_t crc, uint8_t input) {
  const uint32_t polynomial = 0xFFFA0480;
  crc |= input;
  for (uint8_t bit = 0; bit < 8; bit++) {
    if (crc & 0x80000000) {
      crc ^= polynomial;
    }
    crc <<= 1;
  }
  return crc;
}

uint32_t airCrc24(const uint8_t* data, size_t length) {
  uint32_t crc = 0;
  for (size_t i = 0; i < length; i++) {
    crc = crc24Pass(crc, data[i]);
  }
  crc = crc24Pass(crc, 0);
  crc = crc24Pass(crc, 0);
  crc = crc24Pass(crc, 0);
  return crc >> 8;
}

/* The CRC is sent most significant byte first (D.1.2). */
static bool adslCrcMatches(const uint8_t* packet, uint8_t length) {
  uint8_t covered = length - ADSL_CRC_LENGTH;
  const uint8_t* sent = packet + covered;
  uint32_t expected =
      ((uint32_t)sent[0] << 16) | ((uint32_t)sent[1] << 8) | sent[2];
  return airCrc24(packet, covered) == expected;
}

static bool parseFlarm(const uint8_t* raw, size_t rawLength, AirFrame* frame) {
  if (rawLength < 2 * FLARM_FRAME_LENGTH) {
    return false;
  }
  uint8_t decoded[FLARM_FRAME_LENGTH];
  frame->violations = airManchesterDecode(raw, decoded, FLARM_FRAME_LENGTH);
  frame->protocol = AIR_FLARM;
  frame->length = FLARM_PAYLOAD_LENGTH + FLARM_CRC_LENGTH;
  memcpy(frame->bytes, decoded + sizeof(flarmSyncTail), frame->length);

  /* The CRC covers the three sync bytes as well as the payload: on the nRF905
     early FLARM units were built on they were the address field, which that
     chip's CRC includes. */
  uint16_t crc = airCrc16Ccitt(0xFFFF, flarmSyncTail, sizeof(flarmSyncTail));
  crc = airCrc16Ccitt(crc, frame->bytes, FLARM_PAYLOAD_LENGTH);
  uint16_t sent = ((uint16_t)frame->bytes[FLARM_PAYLOAD_LENGTH] << 8) |
                  frame->bytes[FLARM_PAYLOAD_LENGTH + 1];
  frame->check = crc == sent ? 1 : 0;
  return true;
}

static bool parseMbandAdsl(const uint8_t* raw, size_t rawLength, uint8_t length,
                           AirFrame* frame) {
  size_t total = sizeof(adslSyncWord) + 1 + length;
  if (length < ADSL_MIN_LENGTH || total > MBAND_MAX_DECODED ||
      2 * total > rawLength) {
    return false;
  }
  uint8_t decoded[MBAND_MAX_DECODED];
  frame->violations = airManchesterDecode(raw, decoded, total);
  frame->protocol = AIR_ADSL;
  frame->length = 1 + length;
  memcpy(frame->bytes, decoded + sizeof(adslSyncWord), frame->length);
  frame->check = adslCrcMatches(frame->bytes + 1, length) ? 1 : 0;
  return true;
}

/* Only the first three bytes decide which protocol it is, and they have to
   decode cleanly: a chip error there is as likely to be noise that happened to
   match 0xF5 as a frame, and nothing after it can be trusted to say. */
static bool parseMband(const uint8_t* raw, size_t rawLength, AirFrame* frame) {
  uint8_t head[3];
  if (rawLength < 2 * sizeof(head) ||
      airManchesterDecode(raw, head, sizeof(head)) != 0) {
    return false;
  }
  if (memcmp(head, flarmSyncTail, sizeof(flarmSyncTail)) == 0) {
    return parseFlarm(raw, rawLength, frame);
  }
  if (memcmp(head, adslSyncWord, sizeof(adslSyncWord)) == 0) {
    return parseMbandAdsl(raw, rawLength, head[2], frame);
  }
  return false;
}

/* The OGN frame ends in 6 bytes of LDPC parity, which is what an OGN receiver
   checks it with. That check is not written yet, so the frame is kept whole
   and marked unchecked; the eight-byte sync word already makes a false match
   unlikely. */
static bool parseOgn(const uint8_t* raw, size_t rawLength, AirFrame* frame) {
  if (rawLength < 2 * OGN_FRAME_LENGTH) {
    return false;
  }
  frame->violations = airManchesterDecode(raw, frame->bytes, OGN_FRAME_LENGTH);
  frame->protocol = AIR_OGN;
  frame->length = OGN_FRAME_LENGTH;
  return true;
}

/* Not Manchester coded. The capture runs past the packet, so the length byte
   says where it ends. */
static bool parseOband(const uint8_t* raw, size_t rawLength, AirFrame* frame) {
  if (rawLength < 1) {
    return false;
  }
  uint8_t length = raw[0];
  if (length < ADSL_MIN_LENGTH || (size_t)length + 1 > rawLength ||
      (size_t)length + 1 > AIR_FRAME_MAX_BYTES) {
    return false;
  }
  frame->protocol = AIR_ADSL;
  frame->length = 1 + length;
  memcpy(frame->bytes, raw, frame->length);
  frame->check = adslCrcMatches(raw + 1, length) ? 1 : 0;
  return true;
}

/* The chip has already checked the LoRa CRC when the header announced one;
   only the caller knows whether it did, so it says so. */
static bool parseFanet(const uint8_t* raw, size_t rawLength, AirFrame* frame) {
  if (rawLength == 0) {
    return false;
  }
  frame->protocol = AIR_FANET;
  frame->length =
      rawLength > AIR_FRAME_MAX_BYTES ? AIR_FRAME_MAX_BYTES : rawLength;
  memcpy(frame->bytes, raw, frame->length);
  return true;
}

bool airFrameParse(AirListen listen, const uint8_t* raw, size_t rawLength,
                   AirFrame* frame) {
  memset(frame, 0, sizeof(*frame));
  frame->check = -1;
  switch (listen) {
    case AIR_LISTEN_MBAND_LOW:
    case AIR_LISTEN_MBAND_HIGH:
      return parseMband(raw, rawLength, frame);
    case AIR_LISTEN_OGN_LOW:
    case AIR_LISTEN_OGN_HIGH:
      return parseOgn(raw, rawLength, frame);
    case AIR_LISTEN_FANET:
      return parseFanet(raw, rawLength, frame);
    case AIR_LISTEN_OBAND:
      return parseOband(raw, rawLength, frame);
    default:
      return false;
  }
}
