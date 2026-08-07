#include "config.h"
#ifdef THR_LORA_MESH
#include <string.h>

#include "lora/loraBridge.h"
#include "lora/loraMesh.h"
#include "params.h"

/* the serial task's dispatcher: one buffer in, whatever it prints out. Reusing
   it is the whole point - a remote command is the same command, so there is no
   second menu to keep in step with the first. */
void printResult(char* data, Print* output);

/* Captures what a command prints so it can travel back in a RESP instead of
   reaching a console nobody is watching. Overflow is counted rather than
   wrapped: the reply is one frame, and the caller has to be told that what it
   is reading is the beginning of a longer answer. */
class CaptureStream : public Print {
 public:
  size_t write(uint8_t byte) override {
    /* CR would cost an escape in the JSON for nothing: the newline alone
       already separates the lines a command prints */
    if (byte == '\r') {
      return 1;
    }
    if (length < LORA_CONSOLE_MAX_REPLY) {
      buffer[length++] = (char)byte;
    } else {
      overflowed = true;
    }
    return 1;
  }

  char buffer[LORA_CONSOLE_MAX_REPLY];
  uint8_t length = 0;
  boolean overflowed = false;
};

/* One command waits at a time, and it waits outside the receive path.
   Executing it where it arrives would run it before its own receipt is on the
   air - and a command that reboots, or that transmits and then blocks on an
   ACK, would do so from inside the radio's own callback. */
struct ConsoleJob {
  boolean pending;
  uint8_t source;
  uint32_t counter;
  uint8_t budget;
  char text[LORA_CONSOLE_MAX_TEXT + 1];
};

static ConsoleJob job;

uint8_t loraMeshQueueConsole(uint8_t source,
                             uint32_t counter,
                             uint8_t hops,
                             const uint8_t* body,
                             uint8_t bodyLength) {
  if (bodyLength < 2 || bodyLength - 1 > LORA_CONSOLE_MAX_TEXT) {
    return LORA_REASON_BAD_BODY;
  }
  uint8_t length = (uint8_t)(bodyLength - 1);

  /* a command is what an operator would have typed: a lowercase verb, then
     printable ASCII. Anything else never came from the menu this executes. */
  if (body[1] < 'a' || body[1] > 'z') {
    return LORA_REASON_BAD_BODY;
  }
  for (uint8_t i = 0; i < length; i++) {
    if (body[1 + i] < 0x20 || body[1 + i] >= 0x7F) {
      return LORA_REASON_BAD_BODY;
    }
  }

  /* the slot is one deep on purpose: a second command arriving before the
     first has answered would replace the request its RESP is addressed to */
  if (job.pending) {
    return LORA_REASON_BUSY;
  }

  memcpy(job.text, body + 1, length);
  job.text[length] = '\0';
  job.source = source;
  job.counter = counter;
  job.budget = hops;
  job.pending = true;
  return LORA_STATUS_OK;
}

boolean loraMeshRunQueuedConsole(LoraConsoleReply* reply) {
  if (!job.pending) {
    return false;
  }
  job.pending = false;

  Print* json = loraBridgeBegin("exec");
  loraBridgeInt(json, "src", job.source);
  loraBridgeTextBytes(json, "cmd", job.text, (uint8_t)strlen(job.text));
  loraBridgeEnd(json);

  CaptureStream capture;
  printResult(job.text, &capture);

  /* printResult ends every command with a blank line, and most print one of
     their own, so the reply would otherwise spend its scarce bytes on them */
  uint8_t length = capture.length;
  while (length > 0 && (capture.buffer[length - 1] == '\n' ||
                        capture.buffer[length - 1] == ' ')) {
    length--;
  }
  reply->destination = job.source;
  reply->requestCounter = job.counter;
  reply->budget = job.budget;
  reply->body[0] = LORA_CMD_CONSOLE;
  reply->body[1] = capture.overflowed ? LORA_CONSOLE_TRUNCATED : 0;
  memcpy(reply->body + 2, capture.buffer, length);
  reply->bodyLength = (uint8_t)(2 + length);
  return true;
}

void loraMeshReportConsole(uint8_t source,
                           const uint8_t* body,
                           uint8_t bodyLength) {
  if (bodyLength < 2) {
    return;
  }
  boolean truncated = (body[1] & LORA_CONSOLE_TRUNCATED) != 0;
  const char* text = (const char*)body + 2;
  uint8_t length = (uint8_t)(bodyLength - 2);

  Print* json = loraBridgeBegin("console");
  loraBridgeInt(json, "src", source);
  if (json != NULL) {
    loraBridgeTextBytes(json, "text", text, length);
    loraBridgeInt(json, "truncated", truncated ? 1 : 0);
    loraBridgeEnd(json);
    return;
  }

  Serial.print(F("Node "));
  Serial.print(source);
  Serial.print(F(": "));
  for (uint8_t i = 0; i < length; i++) {
    Serial.print(text[i] == '\n' ? ' ' : text[i]);
  }
  if (truncated) {
    Serial.print(F(" ..."));
  }
  Serial.println();
}

/* (ar) - run a console command on another node: ar42:pr1234 types "pr1234" on
   node 42 and brings back what it printed. This is the only CMD that is not a
   parameter, so it is also the only way to reach a verb - a reboot, a peer
   dump, a wifi scan - on a node nobody can plug a cable into. */
void processLoraMeshRunCommand(char* paramValue, Print* output) {
  const char* text = paramValue;
  uint8_t destination;
  if (!loraMeshParseDestination(&text, &destination, output)) {
    return;
  }
  /* the answer is a frame addressed back here, so a broadcast run would have
     every node transmitting its own console at once */
  if (destination == LORA_ADDRESS_BROADCAST) {
    output->println(F("A run needs a destination, e.g. ar42:pr1234"));
    return;
  }

  size_t length = strlen(text);
  if (length == 0 || text[0] < 'a' || text[0] > 'z') {
    output->println(F("Expected a lowercase command, e.g. ar42:pr1234"));
    return;
  }
  if (length > LORA_CONSOLE_MAX_TEXT) {
    output->print(F("A command may not exceed "));
    output->print(LORA_CONSOLE_MAX_TEXT);
    output->println(F(" characters"));
    return;
  }

  uint8_t body[LORA_MAX_BODY_SIZE];
  body[0] = LORA_CMD_CONSOLE;
  memcpy(body + 1, text, length);

  output->print(F("Running "));
  output->print(text);
  output->print(F(" on "));
  output->println(destination);
  loraMeshSend(destination, LORA_TYPE_CMD, body, (uint8_t)(1 + length), output);
}
#endif
