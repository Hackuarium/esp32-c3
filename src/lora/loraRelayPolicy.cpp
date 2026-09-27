#include "loraRelayPolicy.h"

#include <string.h>

#define ECHO_MASK 0xFFFFFFul

bool loraFrameIsReply(const LoraFrame* frame) {
  return frame->type == LORA_TYPE_ACK || frame->type == LORA_TYPE_NACK ||
         frame->type == LORA_TYPE_RESP;
}

bool loraReplyEcho(const LoraFrame* frame, uint32_t* echoed) {
  if (!loraFrameIsReply(frame) || frame->bodyLength < LORA_RESP_COUNTER_SIZE) {
    return false;
  }
  *echoed = ((uint32_t)frame->body[0] << 16) |
            ((uint32_t)frame->body[1] << 8) | (uint32_t)frame->body[2];
  return true;
}

static bool canTravelFurther(const LoraFrame* frame) {
  return frame->budget > frame->hops;
}

bool loraWantsReceipt(const LoraFrame* frame, uint8_t bridgeAddress) {
  if (frame->type != LORA_TYPE_HELLO && frame->type != LORA_TYPE_DATA) {
    return false;
  }
  if (frame->destination != LORA_ADDRESS_BROADCAST &&
      frame->destination != bridgeAddress) {
    return false;
  }
  return canTravelFurther(frame);
}

uint8_t loraExpectedReply(const LoraFrame* frame) {
  switch (frame->type) {
    case LORA_TYPE_HELLO:
    case LORA_TYPE_DATA:
      return LORA_REPLY_ACK;
    case LORA_TYPE_DATA_ACK:
      /* its destination acknowledges it; nobody acknowledges a broadcast */
      return frame->destination == LORA_ADDRESS_BROADCAST ? LORA_REPLY_NONE
                                                          : LORA_REPLY_ACK;
    case LORA_TYPE_CMD:
      if (frame->destination == LORA_ADDRESS_BROADCAST) {
        return LORA_REPLY_NONE;
      }
      if (frame->bodyLength >= 1 &&
          frame->body[0] == LORA_CMD_GET_PARAMETERS) {
        return LORA_REPLY_RESP;
      }
      return LORA_REPLY_ACK;
    default:
      return LORA_REPLY_NONE;
  }
}

void loraRelayedForget(LoraRelayedMemory* memory) {
  memset(memory, 0, sizeof(LoraRelayedMemory));
}

void loraRelayedRemember(LoraRelayedMemory* memory,
                         uint8_t source,
                         uint32_t counter) {
  memory->source[memory->next] = source;
  memory->counter[memory->next] = counter & ECHO_MASK;
  memory->next = (uint8_t)((memory->next + 1) % LORA_RELAYED_MEMORY_SIZE);
  if (memory->count < LORA_RELAYED_MEMORY_SIZE) {
    memory->count++;
  }
}

bool loraReplyOnPath(const LoraRelayedMemory* memory, const LoraFrame* reply) {
  uint32_t echoed;
  if (!loraReplyEcho(reply, &echoed)) {
    return false;
  }
  /* the reply goes back to whoever sent the message it answers */
  for (uint8_t i = 0; i < memory->count; i++) {
    if (memory->source[i] == reply->destination &&
        memory->counter[i] == echoed) {
      return true;
    }
  }
  return false;
}
