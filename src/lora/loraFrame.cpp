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

/* every immutable bit - version, type, counter size - stays bound to the tag;
   only the three ttl bits are masked out, which is unavoidable for any hop
   limited protocol */
static void buildNonce(uint8_t ctrl,
                       uint8_t source,
                       uint8_t destination,
                       uint32_t counter,
                       uint8_t* nonce) {
  memset(nonce, 0, LORA_NONCE_SIZE);
  nonce[0] = ctrl & LORA_CTRL_IMMUTABLE;
  nonce[1] = source;
  nonce[2] = destination;
  /* always the zero extended 32 bit value, so widening the transmitted counter
     from 3 to 4 bytes can never collide with a nonce already used */
  nonce[3] = (uint8_t)(counter >> 24);
  nonce[4] = (uint8_t)(counter >> 16);
  nonce[5] = (uint8_t)(counter >> 8);
  nonce[6] = (uint8_t)counter;
}

/* the header as transmitted, with the ttl bits cleared */
static uint8_t buildAdditionalData(const uint8_t* header,
                                   uint8_t size,
                                   uint8_t* additionalData) {
  memcpy(additionalData, header, size);
  additionalData[0] &= LORA_CTRL_IMMUTABLE;
  return size;
}

uint8_t loraFrameEncode(const LoraFrame* frame,
                        const uint8_t* key,
                        uint8_t* buffer,
                        uint8_t bufferSize) {
  uint8_t counterSize = frame->counter > LORA_COUNTER_24_BIT_MAX ? 4 : 3;
  uint8_t size = counterSize + 3;
  if (frame->bodyLength > LORA_MAX_BODY_SIZE ||
      (uint16_t)size + frame->bodyLength + LORA_MIC_SIZE > bufferSize) {
    return 0;
  }

  uint8_t ctrl = (uint8_t)(((frame->version & 0x01) << 7) |
                           ((frame->type & 0x07) << 4) |
                           ((counterSize == 4 ? 1 : 0) << 3) |
                           (frame->ttl & LORA_TTL_MASK));
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
  uint8_t additionalData[LORA_HEADER_MAX_SIZE];
  buildAdditionalData(buffer, size, additionalData);

  mbedtls_ccm_context context;
  mbedtls_ccm_init(&context);
  int state = mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key,
                                 LORA_KEY_SIZE * 8);
  if (state == 0) {
    state = mbedtls_ccm_encrypt_and_tag(
        &context, frame->bodyLength, nonce, LORA_NONCE_SIZE, additionalData,
        size, frame->body, buffer + size, buffer + size + frame->bodyLength,
        LORA_MIC_SIZE);
  }
  mbedtls_ccm_free(&context);
  if (state != 0) {
    return 0;
  }

  return size + frame->bodyLength + LORA_MIC_SIZE;
}

boolean loraFrameDecode(const uint8_t* buffer,
                        uint8_t length,
                        const uint8_t* key,
                        LoraFrame* frame) {
  if (length < 1) {
    return false;
  }
  uint8_t ctrl = buffer[0];
  uint8_t size = headerSize(ctrl);
  if (length < size + LORA_MIC_SIZE) {
    return false;
  }
  uint8_t bodyLength = length - size - LORA_MIC_SIZE;
  if (bodyLength > LORA_MAX_BODY_SIZE) {
    return false;
  }

  frame->version = (ctrl >> 7) & 0x01;
  frame->type = (ctrl >> 4) & 0x07;
  frame->ttl = ctrl & LORA_TTL_MASK;
  frame->source = buffer[1];
  frame->destination = buffer[2];
  frame->counter = loraFrameGetCounter(buffer);
  frame->bodyLength = bodyLength;

  uint8_t nonce[LORA_NONCE_SIZE];
  buildNonce(ctrl, frame->source, frame->destination, frame->counter, nonce);
  uint8_t additionalData[LORA_HEADER_MAX_SIZE];
  buildAdditionalData(buffer, size, additionalData);

  mbedtls_ccm_context context;
  mbedtls_ccm_init(&context);
  int state = mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key,
                                 LORA_KEY_SIZE * 8);
  if (state == 0) {
    state = mbedtls_ccm_auth_decrypt(&context, bodyLength, nonce,
                                     LORA_NONCE_SIZE, additionalData, size,
                                     buffer + size, frame->body,
                                     buffer + size + bodyLength, LORA_MIC_SIZE);
  }
  mbedtls_ccm_free(&context);

  return state == 0;
}

void loraFrameSetTtl(uint8_t* buffer, uint8_t ttl) {
  buffer[0] = (uint8_t)((buffer[0] & LORA_CTRL_IMMUTABLE) |
                        (ttl & LORA_TTL_MASK));
}

uint8_t loraFrameGetTtl(const uint8_t* buffer) {
  return buffer[0] & LORA_TTL_MASK;
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
