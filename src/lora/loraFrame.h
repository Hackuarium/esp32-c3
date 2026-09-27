#ifndef _LORA_FRAME_H
#define _LORA_FRAME_H

#include <stdbool.h>
#include <stdint.h>

/* Wire format of the private LoRa mesh. One group, one AES-128 key, flooding
   with a hop budget, no routing tables.

     from(1) seal(3|4) | E( ctrl(1) src(1) dst(1) counter(3|4) body(n) route(2h) budget,hops(1) ) | mic(4)
     \__ clear, AAD __/     \___________________________ encrypted ___________________________/

     ctrl, bit 7 to bit 0:  ver(1) type(3) spare(4)
     trailer, bit 7 to 0:   budget(4) hops(4)
     a counter:             big-endian, 3 bytes below 2^23; 4 bytes with bit 7
                            of the first one set above

   Only the transmitter of this copy and its seal travel in clear. Who sent the
   frame, to whom, which message it is, the path it took and the budget it has
   left are all inside the ciphertext, and all authenticated.

   Every transmission is sealed afresh by whoever makes it - the origin, each
   retry, each relay - under a nonce built from its own address and a counter
   it has never used. That is what lets a relay record its passage and a retry
   raise the budget: both change the plaintext, and encrypting two plaintexts
   under one nonce is the misuse CCM does not survive. So a frame carries two
   numbers. The seal only has to be unique. The counter names the message and
   stays the same across every copy and every retry, which is what duplicate
   suppression, anti-replay and the ACK echo all key on.

   The plaintext is read from both ends, which is what makes it self
   describing: ctrl and the counter's first byte give the header size, the last
   byte gives the hop count, the number of stored route entries follows from
   it, and the body is whatever lies between. */

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
/* the format written in ctrl, and refused when it is anything else */
#define LORA_VERSION 1

/* Both counters are written in the same variable width. The first byte's top
   bit says which: clear is 3 bytes holding 23 bits, set is 4 bytes holding 31.
   The widening is permanent and the nonce always holds the full 32 bit value,
   so the transition cannot produce a nonce already used. */
#define LORA_COUNTER_SHORT_MAX 0x7FFFFFul
#define LORA_COUNTER_MAX 0x7FFFFFFFul
#define LORA_COUNTER_WIDE 0x80

/* from(1) and a seal of 3 or 4 bytes */
#define LORA_SEAL_MAX_SIZE 5
/* ctrl, src, dst and a counter of 3 or 4 bytes */
#define LORA_HEADER_MAX_SIZE 7
/* the radio allows 245, but airtime is the real budget: a 48 byte body is
   already ~1.9 s at SF12 */
#define LORA_MAX_BODY_SIZE 48
#define LORA_PLAINTEXT_MIN_SIZE 7
#define LORA_PLAINTEXT_MAX_SIZE \
  (LORA_HEADER_MAX_SIZE + LORA_MAX_BODY_SIZE + LORA_TRAILER_MAX_SIZE)
#define LORA_MAX_FRAME_SIZE \
  (LORA_SEAL_MAX_SIZE + LORA_PLAINTEXT_MAX_SIZE + LORA_MIC_SIZE)
/* what a frame costs beyond its body while both counters are short and no hop
   has been recorded: a 4 byte seal, a 6 byte header, the tag and the trailer */
#define LORA_FRAME_OVERHEAD 15

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

/* SET, but as a list of runs instead of one: the body is a sequence of
   first(1) header(1) values..., repeated until it is exhausted, where the
   header is the count with bit 7 set when the values are int16.

   A SET names one first slot and a list of values, so writing anything with a
   hole in it - a scene touches BB to BO and skips the two geometry slots -
   costs one frame per run, each with its own acknowledged round trip. Runs put
   the whole thing in one frame and skip the holes exactly, without having to
   know what is in them: the alternative is carrying the current value across
   the gap, which needs the node to have reported it first.

   A node that predates this opcode answers LORA_REASON_UNKNOWN_COMMAND rather
   than misreading it, so a sender falls back to one frame per run - and a
   single-run SET still goes out in the old shape, which every node understands. */
#define LORA_CMD_SET_PARAMETER_RUNS 0x05

/* In a run header: the values are int16 rather than int8. */
#define LORA_RUN_INT16 0x80
#define LORA_RUN_COUNT_MASK 0x7F

/* Runs one body may carry. A run is at least three bytes, so the body would
   hold more; this is what the console command parses into. */
#define LORA_MAX_RUNS_PER_FRAME 8
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
  /* the node that sealed this copy and the counter it sealed it with: the
     source on the first transmission, then whoever retried or relayed it */
  uint8_t transmitter;
  uint32_t seal;
  uint8_t type;
  /* hops the origin allows, and hops already taken */
  uint8_t budget;
  uint8_t hops;
  LoraRouteEntry route[LORA_ROUTE_MAX];
  uint8_t routeLength;
  uint8_t source;
  uint8_t destination;
  /* the message, the same in every copy */
  uint32_t counter;
  uint8_t body[LORA_MAX_BODY_SIZE];
  uint8_t bodyLength;
};

/* The bytes loraFrameEncode would produce, without sealing anything - what the
   duty cycle and the timeouts are priced from before a seal is spent. 0 for a
   frame that cannot be encoded: a route length that is not
   min(hops, LORA_ROUTE_MAX), a body or a counter past its limit. */
uint8_t loraFrameSize(const LoraFrame* frame);

/* Serialises frame and seals it under its transmitter and seal. Returns the
   number of bytes to transmit, or 0 if it does not fit buffer or cannot be
   encoded at all.

   The caller owns nonce freshness: the pair (transmitter, seal) must never be
   sealed twice under one key. */
uint8_t loraFrameEncode(const LoraFrame* frame,
                        const uint8_t* key,
                        uint8_t* buffer,
                        uint8_t bufferSize);

/* Verifies the tag and decrypts buffer into frame. Returns false for anything
   that is not authentic group traffic of this version, which is what keeps a
   relay from amplifying injected packets. Nothing in frame is meaningful after
   a false. */
bool loraFrameDecode(const uint8_t* buffer,
                     uint8_t length,
                     const uint8_t* key,
                     LoraFrame* frame);

/* Records this node's passage: counts the hop and, while the table has room,
   appends the address and the dBm it was heard at. A frame at LORA_HOPS_MAX is
   left as it is. */
void loraFrameRecordRelay(LoraFrame* frame, uint8_t address, int16_t rssi);

#ifdef ARDUINO
#include <Arduino.h>
const __FlashStringHelper* loraTypeName(uint8_t type);
#endif

#endif
