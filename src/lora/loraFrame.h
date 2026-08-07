#ifndef _LORA_FRAME_H
#define _LORA_FRAME_H

#include <Arduino.h>

/* Wire format of the private LoRa mesh. One group, one AES-128 key, flooding
   with a hop budget, no routing tables.

     ctrl(1) src(1) dst(1) counter(3 or 4) | ciphertext(n) | mic(4) | route(2h) | budget,hops(1)
     \___________ authenticated _________/   \_ encrypted _/          \________ mutable _______/

     ctrl, bit 7 to bit 0:  ver(1) type(3) cntsz(1) spare(3)
     trailer, bit 7 to 0:   budget(4) hops(4)

   Everything a relay may rewrite lives in the trailer, past the tag, so the
   header is authenticated in full - nothing is masked out of the nonce or the
   additional data. A relay cannot instead record its passage inside the
   ciphertext: it holds the group key and could re-encrypt, but the nonce is
   built from the origin's source and counter, which it must not change, and
   re-encrypting under a nonce already used is exactly the misuse CCM does not
   survive.

   The price is that the trailer is unauthenticated: a route is metadata of the
   same standing as an RSSI reading, not evidence. LORA_TTL_MAX_ACCEPT, not the
   budget, is what actually caps amplification.

   The trailer is read from the end, which is what makes it self describing:
   the last byte gives the hop count, the number of stored route entries follows
   from it, and everything before them is header, ciphertext and tag. */

#define LORA_TTL_MAX 7
/* a relay refuses to forward a frame whose remaining budget is larger than
   this, which caps amplification whatever the sender (or an attacker) claims */
#define LORA_TTL_MAX_ACCEPT 3
/* the budget and the hop count are one nibble each */
#define LORA_HOPS_MASK 0x0F
#define LORA_HOPS_MAX 15
/* Route entries are address(1) rssi(1): the dBm at which that relay heard the
   frame, so a single reception carries the margin of every hop it crossed. The
   last hop is missing on purpose - the receiver measures that one itself. A
   frame that outruns the table keeps counting hops and stops recording them,
   so hops > LORA_ROUTE_MAX is how a truncated route announces itself. */
#define LORA_ROUTE_MAX 4
#define LORA_ROUTE_ENTRY_SIZE 2
#define LORA_TRAILER_MAX_SIZE (1 + LORA_ROUTE_MAX * LORA_ROUTE_ENTRY_SIZE)

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
#define LORA_MAX_FRAME_SIZE                                    \
  (LORA_HEADER_MAX_SIZE + LORA_MAX_BODY_SIZE + LORA_MIC_SIZE + \
   LORA_TRAILER_MAX_SIZE)

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

   SET:     opcode(1) first(1) values(n) - int8 or int16 per the opcode
   GET:     opcode(1) first(1) count(1)
   CONSOLE: opcode(1) text(n)            - printable ASCII, no terminator

   A GET is answered with a RESP frame rather than an ACK, because the caller
   wants the values, not a receipt:

   RESP: echoed request counter, low 24 bits(3) opcode(1) first(1) values(n)

   The counter echo is what lets the requester close its pending request, the
   same trick ACK uses.

   CONSOLE is the one opcode that does not name a parameter: its body is the
   command an operator would have typed on that node's own port, and it is
   answered twice - an ACK the moment it is queued, then a RESP carrying what
   the command printed:

   RESP: echoed counter(3) opcode(1) flags(1) text(n)

   Two answers rather than one because the command runs after the ACK, not
   before: a node told to reboot never gets to send a RESP, and the receipt is
   the only thing that can prove the frame arrived at all. The flags byte exists
   for one bit, LORA_CONSOLE_TRUNCATED, because a reply cut at the frame
   boundary and a command that simply had little to say are otherwise the same
   43 bytes - and reading the first as the second is how an operator concludes a
   node answered when it only started to. */
#define LORA_CMD_SET_PARAMETERS_INT8 0x01
#define LORA_CMD_SET_PARAMETERS_INT16 0x02
#define LORA_CMD_GET_PARAMETERS 0x03
#define LORA_CMD_CONSOLE 0x04
#define LORA_RESP_COUNTER_SIZE 3
/* one frame cannot carry more than this many parameters as int16 */
#define LORA_MAX_PARAMETERS_PER_FRAME 20
/* A console exchange is two frames and both are capped by the body, not by the
   command: what a node prints is unbounded (the parameter dump alone is one
   line per slot) while 44 bytes already cost ~1.8 s at SF12. So the reply is
   truncated to one frame and says so, rather than paging a console over a
   channel that has a duty cycle. */
#define LORA_CONSOLE_MAX_TEXT (LORA_MAX_BODY_SIZE - 1)
#define LORA_CONSOLE_MAX_REPLY (LORA_MAX_BODY_SIZE - LORA_RESP_COUNTER_SIZE - 2)
/* set when the command printed more than the frame could carry */
#define LORA_CONSOLE_TRUNCATED 0x01

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
/* a console command is still waiting to run, and only one slot holds one */
#define LORA_REASON_BUSY 0x04

struct LoraRouteEntry {
  uint8_t address;
  int8_t rssi;
};

struct LoraFrame {
  uint8_t version;
  uint8_t type;
  /* hops the origin allows, and hops already taken */
  uint8_t budget;
  uint8_t hops;
  LoraRouteEntry route[LORA_ROUTE_MAX];
  uint8_t routeLength;
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

/* Rewrites the budget of an already encoded frame. The tag stays valid because
   the trailer is outside the authenticated data, so escalating a retry from 0
   to 2 hops reuses the ciphertext byte for byte instead of re-encrypting. */
void loraFrameSetBudget(uint8_t* buffer, uint8_t length, uint8_t budget);

/* Records this node's passage: counts the hop and, while the table has room,
   appends the address and the dBm it was heard at. The frame grows by one entry
   when the address is stored, so length is updated in place. Returns false if
   the buffer cannot hold the entry, which leaves the frame untouched. */
boolean loraFrameAppendRelay(uint8_t* buffer,
                             uint8_t* length,
                             uint8_t bufferSize,
                             uint8_t address,
                             int16_t rssi);

/* Reads source and counter straight from the header, before decryption, so the
   relay queue can match overheard copies of a frame it is about to forward. */
uint8_t loraFrameGetSource(const uint8_t* buffer);
uint32_t loraFrameGetCounter(const uint8_t* buffer);

const __FlashStringHelper* loraTypeName(uint8_t type);

#endif
