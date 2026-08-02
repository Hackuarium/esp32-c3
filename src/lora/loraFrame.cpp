#include "config.h"
#ifdef THR_LORA_MESH
#include "lora/loraFrame.h"

#include <string.h>
#include "mbedtls/ccm.h"

/* AES-128-CCM with a 4 byte tag: one primitive gives both confidentiality and
   authenticity, so there is no separate CMAC pass and no second key. Four bytes
   put a blind forgery at 2^-32, which at LoRa duty cycles is centuries of
   transmission - the same trade LoRaWAN makes. */

static uint8_t headerSize(uint8_t ctrl) {
  return (ctrl & 0x08) ? 7 : 6;
}

/* The whole header is bound to the tag: everything a relay rewrites sits in the
   trailer, so nothing has to be masked out here and the header as transmitted
   is itself the additional data - no copy, no cleared bits. */
static void buildNonce(uint8_t ctrl,
                       uint8_t source,
                       uint8_t destination,
                       uint32_t counter,
                       uint8_t* nonce) {
  memset(nonce, 0, LORA_NONCE_SIZE);
  nonce[0] = ctrl;
  nonce[1] = source;
  nonce[2] = destination;
  /* always the zero extended 32 bit value, so widening the transmitted counter
     from 3 to 4 bytes can never collide with a nonce already used */
  nonce[3] = (uint8_t)(counter >> 24);
  nonce[4] = (uint8_t)(counter >> 16);
  nonce[5] = (uint8_t)(counter >> 8);
  nonce[6] = (uint8_t)counter;
}

/* the trailer size implied by a hop count: entries stop being recorded once the
   table is full, but the hops keep counting */
static uint8_t trailerSize(uint8_t hops) {
  uint8_t stored = hops < LORA_ROUTE_MAX ? hops : LORA_ROUTE_MAX;
  return (uint8_t)(1 + stored * LORA_ROUTE_ENTRY_SIZE);
}

uint8_t loraFrameEncode(const LoraFrame* frame,
                        const uint8_t* key,
                        uint8_t* buffer,
                        uint8_t bufferSize) {
  uint8_t counterSize = frame->counter > LORA_COUNTER_24_BIT_MAX ? 4 : 3;
  uint8_t size = counterSize + 3;
  /* only an origin encodes, so the trailer is always one byte: no hop has been
     taken yet and there is nothing to record */
  if (frame->bodyLength > LORA_MAX_BODY_SIZE ||
      (uint16_t)size + frame->bodyLength + LORA_MIC_SIZE + 1 > bufferSize) {
    return 0;
  }

  uint8_t ctrl = (uint8_t)(((frame->version & 0x01) << 7) |
                           ((frame->type & 0x07) << 4) |
                           ((counterSize == 4 ? 1 : 0) << 3));
  buffer[0] = ctrl;
  buffer[1] = frame->source;
  buffer[2] = frame->destination;
  if (counterSize == 4) {
    buffer[3] = (uint8_t)(frame->counter >> 24);
    buffer[4] = (uint8_t)(frame->counter >> 16);
    buffer[5] = (uint8_t)(frame->counter >> 8);
    buffer[6] = (uint8_t)frame->counter;
  } else {
    buffer[3] = (uint8_t)(frame->counter >> 16);
    buffer[4] = (uint8_t)(frame->counter >> 8);
    buffer[5] = (uint8_t)frame->counter;
  }

  uint8_t nonce[LORA_NONCE_SIZE];
  buildNonce(ctrl, frame->source, frame->destination, frame->counter, nonce);
  mbedtls_ccm_context context;
  mbedtls_ccm_init(&context);
  int state = mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key,
                                 LORA_KEY_SIZE * 8);
  if (state == 0) {
    state = mbedtls_ccm_encrypt_and_tag(
        &context, frame->bodyLength, nonce, LORA_NONCE_SIZE, buffer,
        size, frame->body, buffer + size, buffer + size + frame->bodyLength,
        LORA_MIC_SIZE);
  }
  mbedtls_ccm_free(&context);
  if (state != 0) {
    return 0;
  }

  uint8_t length = size + frame->bodyLength + LORA_MIC_SIZE;
  buffer[length] = (uint8_t)((frame->budget & LORA_HOPS_MASK) << 4);
  return length + 1;
}

boolean loraFrameDecode(const uint8_t* buffer,
                        uint8_t length,
                        const uint8_t* key,
                        LoraFrame* frame) {
  if (length < 1) {
    return false;
  }
  /* the trailer is read from the end: its last byte says how many hops the
     frame took, which is what gives the size of the rest of it */
  uint8_t trailer = buffer[length - 1];
  uint8_t budget = trailer >> 4;
  uint8_t hops = trailer & LORA_HOPS_MASK;
  uint8_t trailerBytes = trailerSize(hops);
  if (length < trailerBytes) {
    return false;
  }
  uint8_t payloadLength = length - trailerBytes;

  uint8_t ctrl = buffer[0];
  uint8_t size = headerSize(ctrl);
  if (payloadLength < size + LORA_MIC_SIZE) {
    return false;
  }
  uint8_t bodyLength = payloadLength - size - LORA_MIC_SIZE;
  if (bodyLength > LORA_MAX_BODY_SIZE) {
    return false;
  }

  frame->version = (ctrl >> 7) & 0x01;
  frame->type = (ctrl >> 4) & 0x07;
  frame->budget = budget;
  frame->hops = hops;
  frame->routeLength = hops < LORA_ROUTE_MAX ? hops : LORA_ROUTE_MAX;
  for (uint8_t i = 0; i < frame->routeLength; i++) {
    const uint8_t* entry = buffer + payloadLength + i * LORA_ROUTE_ENTRY_SIZE;
    frame->route[i].address = entry[0];
    frame->route[i].rssi = (int8_t)entry[1];
  }
  frame->source = buffer[1];
  frame->destination = buffer[2];
  frame->counter = loraFrameGetCounter(buffer);
  frame->bodyLength = bodyLength;

  uint8_t nonce[LORA_NONCE_SIZE];
  buildNonce(ctrl, frame->source, frame->destination, frame->counter, nonce);
  mbedtls_ccm_context context;
  mbedtls_ccm_init(&context);
  int state = mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key,
                                 LORA_KEY_SIZE * 8);
  if (state == 0) {
    state = mbedtls_ccm_auth_decrypt(&context, bodyLength, nonce,
                                     LORA_NONCE_SIZE, buffer, size,
                                     buffer + size, frame->body,
                                     buffer + size + bodyLength, LORA_MIC_SIZE);
  }
  mbedtls_ccm_free(&context);

  return state == 0;
}

void loraFrameSetBudget(uint8_t* buffer, uint8_t length, uint8_t budget) {
  buffer[length - 1] = (uint8_t)(((budget & LORA_HOPS_MASK) << 4) |
                                 (buffer[length - 1] & LORA_HOPS_MASK));
}

boolean loraFrameAppendRelay(uint8_t* buffer,
                             uint8_t* length,
                             uint8_t bufferSize,
                             uint8_t address,
                             int16_t rssi) {
  uint8_t trailer = buffer[*length - 1];
  uint8_t hops = trailer & LORA_HOPS_MASK;
  if (hops >= LORA_HOPS_MAX) {
    return true;
  }

  if (hops >= LORA_ROUTE_MAX) {
    /* the table is full: keep counting the hop, stop recording it */
    buffer[*length - 1] = (uint8_t)((trailer & 0xF0) | (hops + 1));
    return true;
  }
  if ((uint16_t)*length + LORA_ROUTE_ENTRY_SIZE > bufferSize) {
    return false;
  }

  /* the entry goes where the trailer byte sits, which then moves to the end */
  if (rssi < -128) {
    rssi = -128;
  } else if (rssi > 127) {
    rssi = 127;
  }
  buffer[*length - 1] = address;
  buffer[*length] = (uint8_t)(int8_t)rssi;
  buffer[*length + 1] = (uint8_t)((trailer & 0xF0) | (hops + 1));
  *length = (uint8_t)(*length + LORA_ROUTE_ENTRY_SIZE);
  return true;
}

uint8_t loraFrameGetSource(const uint8_t* buffer) {
  return buffer[1];
}

uint32_t loraFrameGetCounter(const uint8_t* buffer) {
  if (buffer[0] & 0x08) {
    return ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[4] << 16) |
           ((uint32_t)buffer[5] << 8) | (uint32_t)buffer[6];
  }
  return ((uint32_t)buffer[3] << 16) | ((uint32_t)buffer[4] << 8) |
         (uint32_t)buffer[5];
}

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
