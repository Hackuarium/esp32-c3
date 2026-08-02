#ifndef _LORA_FRAME_H
#define _LORA_FRAME_H

#include <Arduino.h>

/* Wire format of the private LoRa mesh. One group, one AES-128 key, flooding
   with a hop countdown, no routing tables.

     ctrl(1) src(1) dst(1) counter(3 or 4) | ciphertext(n) | mic(4)
     \___________ authenticated ________/    \_ encrypted _/

     ctrl, bit 7 to bit 0:
       ver(1) type(3) cntsz(1) ttl(3)

   Only the three ttl bits are mutable, so they are masked out of both the nonce
   and the additional data - a relay has to be able to decrement the countdown
   without invalidating the tag. Everything else, version and message type
   included, is cryptographically bound. */

#define LORA_CTRL_IMMUTABLE 0xF8
#define LORA_TTL_MASK 0x07
#define LORA_TTL_MAX 7
/* a relay refuses to forward a frame that arrives claiming more hops than this,
   which caps amplification whatever the sender (or an attacker) asks for */
#define LORA_TTL_MAX_ACCEPT 3

#define LORA_ADDRESS_BROADCAST 0xFF
/* 0 means "unset" and 255 is the broadcast address, so a node owns anything
   in between */
#define LORA_ADDRESS_MAX 254
#define LORA_KEY_SIZE 16
#define LORA_MIC_SIZE 4
#define LORA_NONCE_SIZE 13
/* header is 6 bytes while the counter is 24 bit, 7 once it widens */
#define LORA_HEADER_MAX_SIZE 7
/* the radio allows 245, but airtime is the real budget: a 48 byte body is
   already ~1.9 s at SF12 */
#define LORA_MAX_BODY_SIZE 48
#define LORA_MAX_FRAME_SIZE \
  (LORA_HEADER_MAX_SIZE + LORA_MAX_BODY_SIZE + LORA_MIC_SIZE)

/* the counter widens permanently once it no longer fits in 24 bits; the nonce
   is always built from the zero extended 32 bit value, so the transition can
   never produce a nonce collision */
#define LORA_COUNTER_24_BIT_MAX 0xFFFFFFul

#define LORA_TYPE_HELLO 0
#define LORA_TYPE_DATA 1
#define LORA_TYPE_DATA_ACK 2
#define LORA_TYPE_ACK 3
#define LORA_TYPE_CMD 4
#define LORA_TYPE_RESP 5
#define LORA_TYPE_NACK 6
#define LORA_TYPE_EXT 7

/* bodies of CMD frames start with an opcode.

   SET: opcode(1) first(1) values(n)   - values are int8 or int16 per the opcode
   GET: opcode(1) first(1) count(1)

   A GET is answered with a RESP frame rather than an ACK, because the caller
   wants the values, not a receipt:

   RESP: echoed request counter, low 24 bits(3) opcode(1) first(1) values(n)

   The counter echo is what lets the requester close its pending request, the
   same trick ACK uses. */
#define LORA_CMD_SET_PARAMETERS_INT8 0x01
#define LORA_CMD_SET_PARAMETERS_INT16 0x02
#define LORA_CMD_GET_PARAMETERS 0x03
#define LORA_RESP_COUNTER_SIZE 3
/* one frame cannot carry more than this many parameters as int16 */
#define LORA_MAX_PARAMETERS_PER_FRAME 20

/* A DATA body is a SET body byte for byte - same opcode, same first index, same
   values - so telemetry needs no encoder and no parser of its own. The frame
   type is what separates them: a CMD is applied by the receiver, a DATA is only
   reported, which is the whole reason a tracker broadcasts its fix as DATA.
   Writing every neighbour's parameter G to this node's latitude is exactly what
   a broadcast SET would do. */

/* status and reason codes carried by ACK and NACK */
#define LORA_STATUS_OK 0x00
#define LORA_REASON_UNKNOWN_COMMAND 0x01
#define LORA_REASON_BAD_BODY 0x02
#define LORA_REASON_OUT_OF_RANGE 0x03

struct LoraFrame {
  uint8_t version;
  uint8_t type;
  uint8_t ttl;
  uint8_t source;
  uint8_t destination;
  uint32_t counter;
  uint8_t body[LORA_MAX_BODY_SIZE];
  uint8_t bodyLength;
};

/* Serialises and encrypts frame into buffer. Returns the number of bytes to
   transmit, or 0 if the body does not fit. */
uint8_t loraFrameEncode(const LoraFrame* frame,
                        const uint8_t* key,
                        uint8_t* buffer,
                        uint8_t bufferSize);

/* Verifies the tag and decrypts buffer into frame. Returns false for anything
   that is not authentic group traffic, which is what keeps a relay from
   amplifying injected packets. */
boolean loraFrameDecode(const uint8_t* buffer,
                        uint8_t length,
                        const uint8_t* key,
                        LoraFrame* frame);

/* Rewrites the ttl of an already encoded frame. The tag stays valid because the
   ttl bits are outside the authenticated data, so a relay never re-encrypts and
   an escalation from ttl 0 to ttl 2 reuses the ciphertext byte for byte. */
void loraFrameSetTtl(uint8_t* buffer, uint8_t ttl);

uint8_t loraFrameGetTtl(const uint8_t* buffer);

/* Reads source and counter straight from the header, before decryption, so the
   relay queue can match overheard copies of a frame it is about to forward. */
uint8_t loraFrameGetSource(const uint8_t* buffer);
uint32_t loraFrameGetCounter(const uint8_t* buffer);

const __FlashStringHelper* loraTypeName(uint8_t type);

#endif
