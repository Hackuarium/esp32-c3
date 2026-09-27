#ifndef _LORA_RELAY_POLICY_H
#define _LORA_RELAY_POLICY_H

#include <stdint.h>

#include "loraFrame.h"

/* How a frame the bridge already has is kept from being repeated.

   A repeater that hears a frame does not carry it on at once: it first waits
   long enough for the frame to be answered - by a bridge's receipt for what is
   meant for the host, by the destination's ACK or RESP for a command - and
   drops its copy when it hears that answer. Only a repeater that hears no
   answer carries the frame further, which is exactly the one whose copy is
   needed.

   The answers themselves travel back only the way the frame came: a repeater
   carries a reply on only if it relayed the frame the reply answers. Every
   reply already echoes that frame's counter, so this needs no field on the
   wire - only a short memory of what this node relayed.

   Nothing here needs Arduino or a radio, so it is tested on the host. */

/* What a repeater waits for before carrying a frame further. */
#define LORA_REPLY_NONE 0
/* a short ACK or NACK - a receipt is one */
#define LORA_REPLY_ACK 1
/* a RESP, which may fill a whole frame */
#define LORA_REPLY_RESP 2

/* how many relayed messages a repeater remembers when deciding whether a reply
   is on its path. A reply comes back within seconds, and a console answer - the
   slowest - within the time its command takes to run */
#define LORA_RELAYED_MEMORY_SIZE 16

struct LoraRelayedMemory {
  uint8_t source[LORA_RELAYED_MEMORY_SIZE];
  uint32_t counter[LORA_RELAYED_MEMORY_SIZE];
  uint8_t next;
  uint8_t count;
};

/* ACK, NACK and RESP: each begins with the low 24 bits of the counter of the
   message it answers. */
bool loraFrameIsReply(const LoraFrame* frame);

/* The counter a reply answers, low 24 bits. False for anything that is not a
   reply, or a body too short to say. */
bool loraReplyEcho(const LoraFrame* frame, uint32_t* echoed);

/* Whether a bridge answers this copy with a receipt: a HELLO or a DATA meant
   for the host - sent to everyone, or to this bridge - that a repeater could
   still carry further. A command is not receipted even when broadcast: it is
   meant for every node, and a receipt would stop it short of them. */
bool loraWantsReceipt(const LoraFrame* frame, uint8_t bridgeAddress);

/* What a repeater expects to hear answer this frame, LORA_REPLY_*. A reply is
   never answered, and neither is a broadcast command. A frame for the host may
   be receipted by a bridge, a command by its destination; a read is answered
   with the values themselves, which can take a whole frame. */
uint8_t loraExpectedReply(const LoraFrame* frame);

void loraRelayedForget(LoraRelayedMemory* memory);

/* Record that this node carried the message (source, counter) further. */
void loraRelayedRemember(LoraRelayedMemory* memory,
                         uint8_t source,
                         uint32_t counter);

/* Whether a repeater may carry this reply further: only one that relayed the
   message it answers, so an answer retraces its request rather than
   flooding. */
bool loraReplyOnPath(const LoraRelayedMemory* memory, const LoraFrame* reply);

#endif
