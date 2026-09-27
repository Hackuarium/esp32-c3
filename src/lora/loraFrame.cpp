#include "loraFrame.h"

#include <string.h>

#include "mbedtls/ccm.h"

/* AES-128-CCM with a 4 byte tag: one primitive gives both confidentiality and
   authenticity, so there is no separate CMAC pass and no second key. Four bytes
   put a blind forgery at 2^-32, which at LoRa duty cycles is centuries of
   transmission - the same trade LoRaWAN makes.

   Nothing here needs Arduino or a radio, so it is tested on the host against
   the same mbedtls calls. */

/* First byte of every nonce. The previous format began its nonce with a ctrl
   byte whose three low bits were always clear, so no nonce built here can
   repeat one built there, even under the same key and the same counters. */
#define LORA_NONCE_TAG 0x01

static uint8_t storedHops(uint8_t hops) {
  return hops < LORA_ROUTE_MAX ? hops : LORA_ROUTE_MAX;
}

static uint8_t writeCounter(uint8_t* out, uint32_t counter) {
  if (counter > LORA_COUNTER_SHORT_MAX) {
    out[0] = (uint8_t)(LORA_COUNTER_WIDE | (counter >> 24));
    out[1] = (uint8_t)(counter >> 16);
    out[2] = (uint8_t)(counter >> 8);
    out[3] = (uint8_t)counter;
    return 4;
  }
  out[0] = (uint8_t)(counter >> 16);
  out[1] = (uint8_t)(counter >> 8);
  out[2] = (uint8_t)counter;
  return 3;
}

/* Returns the bytes the counter took, or 0 when fewer are available. */
static uint8_t readCounter(const uint8_t* in,
                           uint8_t available,
                           uint32_t* counter) {
  if (available < 3) {
    return 0;
  }
  if (in[0] & LORA_COUNTER_WIDE) {
    if (available < 4) {
      return 0;
    }
    *counter = ((uint32_t)(in[0] & 0x7F) << 24) | ((uint32_t)in[1] << 16) |
               ((uint32_t)in[2] << 8) | (uint32_t)in[3];
    return 4;
  }
  *counter = ((uint32_t)in[0] << 16) | ((uint32_t)in[1] << 8) | (uint32_t)in[2];
  return 3;
}

static void buildNonce(uint8_t transmitter, uint32_t seal, uint8_t* nonce) {
  memset(nonce, 0, LORA_NONCE_SIZE);
  nonce[0] = LORA_NONCE_TAG;
  nonce[1] = transmitter;
  nonce[2] = (uint8_t)(seal >> 24);
  nonce[3] = (uint8_t)(seal >> 16);
  nonce[4] = (uint8_t)(seal >> 8);
  nonce[5] = (uint8_t)seal;
}

uint8_t loraFrameSize(const LoraFrame* frame) {
  uint8_t stored = storedHops(frame->hops);
  if (frame->bodyLength > LORA_MAX_BODY_SIZE || frame->routeLength != stored ||
      frame->hops > LORA_HOPS_MAX || frame->budget > LORA_HOPS_MAX ||
      frame->counter > LORA_COUNTER_MAX || frame->seal > LORA_COUNTER_MAX) {
    return 0;
  }
  uint8_t sealSize = frame->seal > LORA_COUNTER_SHORT_MAX ? 5 : 4;
  uint8_t headerSize = frame->counter > LORA_COUNTER_SHORT_MAX ? 7 : 6;
  return (uint8_t)(sealSize + headerSize + frame->bodyLength +
                   stored * LORA_ROUTE_ENTRY_SIZE + 1 + LORA_MIC_SIZE);
}

uint8_t loraFrameEncode(const LoraFrame* frame,
                        const uint8_t* key,
                        uint8_t* buffer,
                        uint8_t bufferSize) {
  uint8_t size = loraFrameSize(frame);
  if (size == 0 || size > bufferSize) {
    return 0;
  }

  uint8_t stored = storedHops(frame->hops);
  uint8_t plain[LORA_PLAINTEXT_MAX_SIZE];
  plain[0] = (uint8_t)((LORA_VERSION << 7) | ((frame->type & 0x07) << 4));
  plain[1] = frame->source;
  plain[2] = frame->destination;
  uint8_t plainLength = (uint8_t)(3 + writeCounter(plain + 3, frame->counter));
  memcpy(plain + plainLength, frame->body, frame->bodyLength);
  plainLength += frame->bodyLength;
  for (uint8_t i = 0; i < stored; i++) {
    plain[plainLength++] = frame->route[i].address;
    plain[plainLength++] = (uint8_t)frame->route[i].rssi;
  }
  plain[plainLength++] = (uint8_t)((frame->budget << 4) | frame->hops);

  buffer[0] = frame->transmitter;
  uint8_t sealSize = (uint8_t)(1 + writeCounter(buffer + 1, frame->seal));

  /* the clear bytes are the additional data as transmitted: the nonce already
     holds their values, and binding their encoding as well leaves no second
     spelling of a counter that would still verify */
  uint8_t nonce[LORA_NONCE_SIZE];
  buildNonce(frame->transmitter, frame->seal, nonce);
  mbedtls_ccm_context context;
  mbedtls_ccm_init(&context);
  int state = mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key,
                                 LORA_KEY_SIZE * 8);
  if (state == 0) {
    state = mbedtls_ccm_encrypt_and_tag(
        &context, plainLength, nonce, LORA_NONCE_SIZE, buffer, sealSize, plain,
        buffer + sealSize, buffer + sealSize + plainLength, LORA_MIC_SIZE);
  }
  mbedtls_ccm_free(&context);
  return state == 0 ? size : 0;
}

bool loraFrameDecode(const uint8_t* buffer,
                     uint8_t length,
                     const uint8_t* key,
                     LoraFrame* frame) {
  if (length < 1) {
    return false;
  }
  uint32_t seal;
  uint8_t sealCounterSize = readCounter(buffer + 1, length - 1, &seal);
  if (sealCounterSize == 0) {
    return false;
  }
  uint8_t sealSize = (uint8_t)(1 + sealCounterSize);
  if (length < sealSize + LORA_PLAINTEXT_MIN_SIZE + LORA_MIC_SIZE) {
    return false;
  }
  uint8_t plainLength = (uint8_t)(length - sealSize - LORA_MIC_SIZE);
  if (plainLength > LORA_PLAINTEXT_MAX_SIZE) {
    return false;
  }

  uint8_t plain[LORA_PLAINTEXT_MAX_SIZE];
  uint8_t nonce[LORA_NONCE_SIZE];
  buildNonce(buffer[0], seal, nonce);
  mbedtls_ccm_context context;
  mbedtls_ccm_init(&context);
  int state = mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key,
                                 LORA_KEY_SIZE * 8);
  if (state == 0) {
    state = mbedtls_ccm_auth_decrypt(&context, plainLength, nonce,
                                     LORA_NONCE_SIZE, buffer, sealSize,
                                     buffer + sealSize, plain,
                                     buffer + sealSize + plainLength,
                                     LORA_MIC_SIZE);
  }
  mbedtls_ccm_free(&context);
  if (state != 0) {
    return false;
  }

  /* only bytes the tag vouched for are parsed below */
  uint8_t ctrl = plain[0];
  if ((ctrl >> 7) != LORA_VERSION) {
    return false;
  }
  uint32_t counter;
  uint8_t counterSize = readCounter(plain + 3, plainLength - 3, &counter);
  if (counterSize == 0) {
    return false;
  }
  uint8_t trailer = plain[plainLength - 1];
  uint8_t hops = trailer & LORA_HOPS_MASK;
  uint8_t stored = storedHops(hops);
  uint8_t headerSize = (uint8_t)(3 + counterSize);
  uint16_t framing = headerSize + stored * LORA_ROUTE_ENTRY_SIZE + 1;
  if (framing > plainLength) {
    return false;
  }
  uint8_t bodyLength = (uint8_t)(plainLength - framing);
  if (bodyLength > LORA_MAX_BODY_SIZE) {
    return false;
  }

  frame->transmitter = buffer[0];
  frame->seal = seal;
  frame->type = (ctrl >> 4) & 0x07;
  frame->budget = trailer >> 4;
  frame->hops = hops;
  frame->source = plain[1];
  frame->destination = plain[2];
  frame->counter = counter;
  frame->bodyLength = bodyLength;
  memcpy(frame->body, plain + headerSize, bodyLength);
  frame->routeLength = stored;
  const uint8_t* entry = plain + headerSize + bodyLength;
  for (uint8_t i = 0; i < stored; i++) {
    frame->route[i].address = entry[0];
    frame->route[i].rssi = (int8_t)entry[1];
    entry += LORA_ROUTE_ENTRY_SIZE;
  }
  return true;
}

void loraFrameRecordRelay(LoraFrame* frame, uint8_t address, int16_t rssi) {
  if (frame->hops >= LORA_HOPS_MAX) {
    return;
  }
  /* past the table the hop is counted and not recorded */
  if (frame->hops < LORA_ROUTE_MAX) {
    if (rssi < -128) {
      rssi = -128;
    } else if (rssi > 127) {
      rssi = 127;
    }
    frame->route[frame->hops].address = address;
    frame->route[frame->hops].rssi = (int8_t)rssi;
    frame->routeLength = (uint8_t)(frame->hops + 1);
  }
  frame->hops++;
}

#ifdef ARDUINO
const __FlashStringHelper* loraTypeName(uint8_t type) {
  switch (type) {
    case LORA_TYPE_HELLO:
      return F("HELLO");
    case LORA_TYPE_DATA:
      return F("DATA");
    case LORA_TYPE_DATA_ACK:
      return F("DATA_ACK");
    case LORA_TYPE_ACK:
      return F("ACK");
    case LORA_TYPE_CMD:
      return F("CMD");
    case LORA_TYPE_RESP:
      return F("RESP");
    case LORA_TYPE_NACK:
      return F("NACK");
    default:
      return F("EXT");
  }
}
#endif
