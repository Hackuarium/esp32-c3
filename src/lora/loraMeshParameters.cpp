#include "config.h"
#ifdef THR_LORA_MESH
#include <stdlib.h>
#include <string.h>

#include "lora/loraBridge.h"
#include "lora/loraMesh.h"
#include "params.h"

/* Parses the "42:" that may prefix a command and leaves text on the rest. An
   empty prefix ("ac:C6") is a deliberate broadcast, so it is not an error. */
boolean loraMeshParseDestination(const char** text,
                                 uint8_t* destination,
                                 Print* output) {
  const char* colon = strchr(*text, ':');
  *destination = LORA_ADDRESS_BROADCAST;
  if (colon == NULL) {
    return true;
  }
  if (colon != *text) {
    int32_t value = atol(*text);
    if (value < 1 || value > LORA_ADDRESS_MAX) {
      output->print(F("Destination must be 1 to "));
      output->println(LORA_ADDRESS_MAX);
      return false;
    }
    *destination = (uint8_t)value;
  }
  *text = colon + 1;
  return true;
}

/* One or two uppercase letters, using the same A..Z, AA.. scheme as the local
   serial syntax, so "C" is index 2 and "AA" is index 26 */
static boolean parseParameterIndex(const char** text, uint8_t* index) {
  uint16_t letters = 0;
  uint8_t letterCount = 0;
  while (**text >= 'A' && **text <= 'Z' && letterCount < 2) {
    letters = (uint16_t)(letters * 26 + (**text - 'A' + 1));
    letterCount++;
    (*text)++;
  }
  if (letterCount == 0 || letters > MAX_PARAM) {
    return false;
  }
  *index = (uint8_t)(letters - 1);
  return true;
}

/* The "8" of "ac42:DA8" - how many slots follow the first one, 1 when there are
   no digits at all. Clears *valid when the tail is not a plain number. */
static uint16_t parseBlockCount(const char* text, boolean* valid) {
  uint16_t count = 0;
  uint8_t digits = 0;
  while (*text >= '0' && *text <= '9') {
    count = (uint16_t)(count * 10 + (*text - '0'));
    digits++;
    text++;
  }
  *valid = *text == '\0';
  return digits == 0 ? 1 : count;
}

/* "123" or "-5,10,0": the same comma separated form the local serial syntax
   accepts, so the remote command reads like the local one */
/* Lays out opcode, first index and the values themselves. int8 is used when
   every value fits, so the common case of small settings costs one byte each
   instead of two - the opcode tells the receiver which it is looking at. */
static uint8_t encodeValues(uint8_t firstParameter,
                            const int16_t* values,
                            uint8_t count,
                            uint8_t* out,
                            uint8_t outSize) {
  boolean fitsInBytes = true;
  for (uint8_t i = 0; i < count; i++) {
    if (values[i] < -128 || values[i] > 127) {
      fitsInBytes = false;
      break;
    }
  }

  uint8_t needed = 2 + (fitsInBytes ? count : (uint8_t)(count * 2));
  if (needed > outSize) {
    return 0;
  }

  out[0] = fitsInBytes ? LORA_CMD_SET_PARAMETERS_INT8
                       : LORA_CMD_SET_PARAMETERS_INT16;
  out[1] = firstParameter;
  uint8_t length = 2;
  for (uint8_t i = 0; i < count; i++) {
    if (fitsInBytes) {
      out[length++] = (uint8_t)(int8_t)values[i];
    } else {
      out[length++] = lowByte(values[i]);
      out[length++] = highByte(values[i]);
    }
  }
  return length;
}

/* The same layout repeated: first, then the count with bit 7 telling int16 from
   int8, then the values. Each run picks its own width, so a scene whose colours
   need 255 does not force two bytes onto the settings that fit in one. */
static uint8_t encodeRuns(const uint8_t* firsts,
                          const uint8_t* counts,
                          uint8_t runCount,
                          const int16_t* values,
                          uint8_t* out,
                          uint8_t outSize) {
  if (outSize < 1) {
    return 0;
  }
  out[0] = LORA_CMD_SET_PARAMETER_RUNS;
  uint8_t length = 1;
  uint8_t valueIndex = 0;

  for (uint8_t run = 0; run < runCount; run++) {
    uint8_t count = counts[run];
    boolean wide = false;
    for (uint8_t i = 0; i < count; i++) {
      int16_t value = values[valueIndex + i];
      if (value < -128 || value > 127) {
        wide = true;
        break;
      }
    }
    uint16_t needed = 2u + (wide ? (uint16_t)count * 2u : count);
    if (length + needed > outSize) {
      return 0;
    }
    out[length++] = firsts[run];
    out[length++] = (uint8_t)(count | (wide ? LORA_RUN_INT16 : 0));
    for (uint8_t i = 0; i < count; i++) {
      int16_t value = values[valueIndex + i];
      if (wide) {
        out[length++] = lowByte(value);
        out[length++] = highByte(value);
      } else {
        out[length++] = (uint8_t)(int8_t)value;
      }
    }
    valueIndex += count;
  }
  return length;
}

uint8_t loraMeshEncodeLocalParameters(uint8_t firstParameter,
                                      uint8_t count,
                                      uint8_t* out,
                                      uint8_t outSize) {
  if (count == 0 || count > LORA_MAX_PARAMETERS_PER_FRAME ||
      (uint16_t)firstParameter + count > MAX_PARAM) {
    return 0;
  }
  int16_t values[LORA_MAX_PARAMETERS_PER_FRAME];
  for (uint8_t i = 0; i < count; i++) {
    values[i] = getParameter(firstParameter + i);
  }
  return encodeValues(firstParameter, values, count, out, outSize);
}

/* How many parameters a SET-shaped body carries, 0 if it is not one. */
static uint8_t bodyParameterCount(const uint8_t* body, uint8_t bodyLength) {
  if (bodyLength < 3) {
    return 0;
  }
  uint8_t payloadLength = bodyLength - 2;
  if (body[0] == LORA_CMD_SET_PARAMETERS_INT8) {
    return payloadLength;
  }
  if (body[0] == LORA_CMD_SET_PARAMETERS_INT16 && !(payloadLength & 0x01)) {
    return payloadLength / 2;
  }
  return 0;
}

static int16_t bodyValueAt(const uint8_t* body, uint8_t index) {
  if (body[0] == LORA_CMD_SET_PARAMETERS_INT8) {
    return (int8_t)body[2 + index];
  }
  return (int16_t)((uint16_t)body[2 + index * 2] |
                   ((uint16_t)body[3 + index * 2] << 8));
}

int16_t loraMeshParameterFromBody(const uint8_t* body,
                                  uint8_t bodyLength,
                                  uint8_t number) {
  uint8_t count = bodyParameterCount(body, bodyLength);
  if (count == 0 || number < body[1] || number >= body[1] + count) {
    return ERROR_VALUE;
  }
  return bodyValueAt(body, number - body[1]);
}

/* Walks the runs of a LORA_CMD_SET_PARAMETER_RUNS body, applying them only on
   the second pass. A body that is malformed half way through would otherwise
   leave the node with half a scene and a NACK saying it failed, which is worse
   than either outcome on its own. */
static uint8_t applyParameterRuns(const uint8_t* body,
                                  uint8_t bodyLength,
                                  boolean apply) {
  uint8_t index = 1;
  while (index < bodyLength) {
    if ((uint16_t)index + 2 > bodyLength) {
      return LORA_REASON_BAD_BODY;
    }
    uint8_t first = body[index];
    uint8_t header = body[index + 1];
    uint8_t count = header & LORA_RUN_COUNT_MASK;
    boolean wide = (header & LORA_RUN_INT16) != 0;
    index += 2;

    if (count == 0) {
      return LORA_REASON_BAD_BODY;
    }
    uint16_t needed = wide ? (uint16_t)count * 2u : count;
    if ((uint16_t)index + needed > bodyLength) {
      return LORA_REASON_BAD_BODY;
    }
    if ((uint16_t)first + count > MAX_PARAM) {
      return LORA_REASON_OUT_OF_RANGE;
    }
    if (apply) {
      for (uint8_t i = 0; i < count; i++) {
        int16_t value =
            wide ? (int16_t)((uint16_t)body[index + i * 2] |
                             ((uint16_t)body[index + i * 2 + 1] << 8))
                 : (int16_t)(int8_t)body[index + i];
        setAndSaveParameter(first + i, value);
      }
    }
    index += needed;
  }
  return LORA_STATUS_OK;
}

uint8_t loraMeshApplyCommand(const uint8_t* body, uint8_t bodyLength) {
  if (bodyLength < 3) {
    return LORA_REASON_BAD_BODY;
  }
  uint8_t opcode = body[0];

  if (opcode == LORA_CMD_SET_PARAMETER_RUNS) {
    uint8_t status = applyParameterRuns(body, bodyLength, false);
    if (status != LORA_STATUS_OK) {
      return status;
    }
    return applyParameterRuns(body, bodyLength, true);
  }

  uint8_t firstParameter = body[1];
  uint8_t payloadLength = bodyLength - 2;

  if (opcode != LORA_CMD_SET_PARAMETERS_INT8 &&
      opcode != LORA_CMD_SET_PARAMETERS_INT16) {
    return LORA_REASON_UNKNOWN_COMMAND;
  }
  if (opcode == LORA_CMD_SET_PARAMETERS_INT16 && (payloadLength & 0x01)) {
    return LORA_REASON_BAD_BODY;
  }

  uint8_t count = opcode == LORA_CMD_SET_PARAMETERS_INT8 ? payloadLength
                                                         : payloadLength / 2;
  if ((uint16_t)firstParameter + count > MAX_PARAM) {
    return LORA_REASON_OUT_OF_RANGE;
  }

  for (uint8_t i = 0; i < count; i++) {
    setAndSaveParameter(firstParameter + i, bodyValueAt(body, i));
  }
  return LORA_STATUS_OK;
}

/* A pair of int16 halves is unreadable on a console and useless in a database,
   so a block that happens to cover the fix is decorated with the degrees, and
   with the dilution of precision the position has to be weighted by. This is
   the only place in the mesh that knows a parameter means something. */
static void reportGpsFix(Print* json,
                         const uint8_t* body,
                         uint8_t bodyLength) {
#ifdef PARAM_GPS_LATITUDE
  int16_t latitudeLow =
      loraMeshParameterFromBody(body, bodyLength, PARAM_GPS_LATITUDE);
  int16_t latitudeHigh =
      loraMeshParameterFromBody(body, bodyLength, PARAM_GPS_LATITUDE + 1);
  int16_t longitudeLow =
      loraMeshParameterFromBody(body, bodyLength, PARAM_GPS_LONGITUDE);
  int16_t longitudeHigh =
      loraMeshParameterFromBody(body, bodyLength, PARAM_GPS_LONGITUDE + 1);
  if (latitudeLow != ERROR_VALUE && latitudeHigh != ERROR_VALUE &&
      longitudeLow != ERROR_VALUE && longitudeHigh != ERROR_VALUE) {
    double latitude =
        (((int32_t)latitudeLow & 0xFFFF) | ((int32_t)latitudeHigh << 16)) / 1e6;
    double longitude =
        (((int32_t)longitudeLow & 0xFFFF) | ((int32_t)longitudeHigh << 16)) /
        1e6;
    if (json == NULL) {
      Serial.print(F("  fix "));
      Serial.print(latitude, 6);
      Serial.print(F(", "));
      Serial.println(longitude, 6);
    } else {
      loraBridgeFloat(json, "lat", latitude, 6);
      loraBridgeFloat(json, "lon", longitude, 6);
    }
  }
#endif

#ifdef PARAM_GPS_HDOP
  int16_t hdop = loraMeshParameterFromBody(body, bodyLength, PARAM_GPS_HDOP);
  if (hdop != ERROR_VALUE) {
    if (json == NULL) {
      Serial.print(F("  hdop "));
      Serial.println(hdop / 100.0, 2);
    } else {
      loraBridgeFloat(json, "hdop", hdop / 100.0, 2);
    }
  }
#endif

#if !defined(PARAM_GPS_LATITUDE) && !defined(PARAM_GPS_HDOP)
  (void)json;
  (void)body;
  (void)bodyLength;
#endif
}

void loraMeshReportParameters(uint8_t source,
                              const uint8_t* body,
                              uint8_t bodyLength) {
  if (bodyLength < 3) {
    return;
  }
  uint8_t firstParameter = body[1];
  uint8_t count = bodyParameterCount(body, bodyLength);
  if (count == 0) {
    return;
  }

  Print* json = loraBridgeBegin("params");
  loraBridgeInt(json, "src", source);
  if (json == NULL) {
    Serial.print(F("Node "));
    Serial.print(source);
    Serial.print(':');
  }

  for (uint8_t i = 0; i < count; i++) {
    int16_t value = bodyValueAt(body, i);
    String label = numberToLabel(firstParameter + i);
    if (json == NULL) {
      Serial.print(' ');
      Serial.print(label);
      Serial.print('=');
      Serial.print(value);
    } else {
      loraBridgeInt(json, label.c_str(), value);
    }
  }
  if (json == NULL) {
    Serial.println();
  }

  reportGpsFix(json, body, bodyLength);
  loraBridgeEnd(json);
}

/* (ax) - set or read explicit values, mirroring the local serial syntax:
   axA123 broadcasts "A = 123", ax42:A123 sends it to one node and waits for the
   ACK, ax42:A asks node 42 what its A is. */
void processLoraMeshSetCommand(char* paramValue, Print* output) {
  const char* text = paramValue;
  uint8_t destination;
  if (!loraMeshParseDestination(&text, &destination, output)) {
    return;
  }

  /* the same parser the serial console uses, so a command means the same thing
     typed on a port and sent through the mesh */
  ParameterAssignment items[MAX_PARAM_ASSIGNMENTS];
  uint8_t wireAddress = 0;
  uint8_t itemCount =
      parseParameterAssignments(text, items, MAX_PARAM_ASSIGNMENTS, &wireAddress);
  if (itemCount == 0 || wireAddress != 0) {
    output->println(F("Expected e.g. axA123, ax42:BB1,17,1,A1,2,3 or ax42:A"));
    return;
  }

  uint8_t firstParameter = items[0].slot;
  boolean isRead = !items[0].hasValue;

  /* consecutive slots become one run; a jump starts the next */
  uint8_t firsts[LORA_MAX_RUNS_PER_FRAME];
  uint8_t counts[LORA_MAX_RUNS_PER_FRAME];
  int16_t values[LORA_MAX_PARAMETERS_PER_FRAME];
  uint8_t runCount = 0;
  uint8_t count = 0;
  if (!isRead) {
    for (uint8_t i = 0; i < itemCount; i++) {
      if (!items[i].hasValue) {
        output->println(F("A read takes one slot, e.g. ax42:A"));
        return;
      }
      boolean continues = runCount > 0 &&
                          items[i].slot ==
                              (uint16_t)firsts[runCount - 1] + counts[runCount - 1];
      if (continues) {
        counts[runCount - 1]++;
      } else {
        if (runCount >= LORA_MAX_RUNS_PER_FRAME) {
          output->println(F("Too many runs for one frame"));
          return;
        }
        firsts[runCount] = items[i].slot;
        counts[runCount] = 1;
        runCount++;
      }
      values[i] = items[i].value;
    }
    count = counts[0];
  }

  if (isRead) {
    /* no value means a read, and a read has to be addressed: every node
       answering one broadcast at once is a response storm */
    if (destination == LORA_ADDRESS_BROADCAST) {
      output->println(F("A read needs a destination, e.g. ax42:A"));
      return;
    }
    uint8_t body[3];
    body[0] = LORA_CMD_GET_PARAMETERS;
    body[1] = firstParameter;
    body[2] = 1;
    output->print(F("Reading "));
    output->print(numberToLabel(firstParameter));
    output->print(F(" from "));
    output->println(destination);
    loraMeshSend(destination, LORA_TYPE_CMD, body, sizeof(body), output);
    return;
  }

  uint8_t body[LORA_MAX_BODY_SIZE];
  /* one run goes out in the shape every node has always understood; only a
     write with a hole in it needs the opcode that predates nothing */
  uint8_t length =
      runCount > 1
          ? encodeRuns(firsts, counts, runCount, values, body,
                       LORA_MAX_BODY_SIZE)
          : encodeValues(firstParameter, values, count, body,
                         LORA_MAX_BODY_SIZE);
  if (length == 0) {
    output->println(F("Too many parameters for one frame"));
    return;
  }

  output->print(F("Setting "));
  for (uint8_t run = 0; run < runCount; run++) {
    if (run > 0) {
      output->print(F(", "));
    }
    output->print(numberToLabel(firsts[run]));
    if (counts[run] > 1) {
      output->print('+');
      output->print(counts[run] - 1);
    }
  }
  output->print(F(" on "));
  if (destination == LORA_ADDRESS_BROADCAST) {
    output->println(F("every node"));
  } else {
    output->println(destination);
  }
  loraMeshSend(destination, LORA_TYPE_CMD, body, length, output);
}

/* (ag) - read a whole block back in one exchange: ag42:DA8 asks node 42 for DA
   to DH. The GET body has always carried a count and the receiver has always
   honoured it; only the (ax) read form, which fixes it at 1, could not say so.
   One RESP then answers the block, so reading a settings page costs a single
   round trip instead of one per slot - and only this node needs the new code. */
void processLoraMeshGetCommand(char* paramValue, Print* output) {
  const char* text = paramValue;
  uint8_t destination;
  if (!loraMeshParseDestination(&text, &destination, output)) {
    return;
  }
  /* a read has to be addressed: every node answering one broadcast at once is a
     response storm */
  if (destination == LORA_ADDRESS_BROADCAST) {
    output->println(F("A read needs a destination, e.g. ag42:DA8"));
    return;
  }

  uint8_t firstParameter;
  if (!parseParameterIndex(&text, &firstParameter)) {
    output->println(F("Expected e.g. ag42:DA8"));
    return;
  }

  boolean valid = false;
  uint16_t count = parseBlockCount(text, &valid);
  if (!valid) {
    output->println(F("Expected e.g. ag42:DA8"));
    return;
  }

  if (count == 0 || count > LORA_MAX_PARAMETERS_PER_FRAME ||
      (uint16_t)firstParameter + count > MAX_PARAM) {
    output->println(F("Parameter range out of bounds"));
    return;
  }

  uint8_t body[3];
  body[0] = LORA_CMD_GET_PARAMETERS;
  body[1] = firstParameter;
  body[2] = (uint8_t)count;
  output->print(F("Reading "));
  output->print(count);
  output->print(F(" parameter(s) from "));
  output->print(numberToLabel(firstParameter));
  output->print(F(" on "));
  output->println(destination);
  loraMeshSend(destination, LORA_TYPE_CMD, body, sizeof(body), output);
}

/* (ac) - copy this node's own values outwards: ac42:C6 pushes C to H to node
   42, ac:C6 pushes them to everyone. */
void processLoraMeshCopyCommand(char* paramValue, Print* output) {
  const char* text = paramValue;
  uint8_t destination;
  if (!loraMeshParseDestination(&text, &destination, output)) {
    return;
  }

  uint8_t firstParameter;
  if (!parseParameterIndex(&text, &firstParameter)) {
    output->println(F("Expected e.g. acC6 or ac42:C6"));
    return;
  }

  boolean valid = false;
  uint16_t count = parseBlockCount(text, &valid);
  if (!valid) {
    output->println(F("Expected e.g. acC6 or ac42:C6"));
    return;
  }

  if (count == 0 || count > LORA_MAX_PARAMETERS_PER_FRAME ||
      (uint16_t)firstParameter + count > MAX_PARAM) {
    output->println(F("Parameter range out of bounds"));
    return;
  }

  uint8_t body[LORA_MAX_BODY_SIZE];
  uint8_t length = loraMeshEncodeLocalParameters(firstParameter, (uint8_t)count,
                                                 body, LORA_MAX_BODY_SIZE);
  if (length == 0) {
    output->println(F("Too many parameters for one frame"));
    return;
  }

  output->print(F("Copying "));
  output->print(count);
  output->print(F(" parameter(s) from "));
  output->print(numberToLabel(firstParameter));
  output->print(F(" to "));
  if (destination == LORA_ADDRESS_BROADCAST) {
    output->println(F("every node"));
  } else {
    output->println(destination);
  }
  loraMeshSend(destination, LORA_TYPE_CMD, body, length, output);
}

/* The defaults of the reserved block live here rather than in a main file
   because a board joining the mesh is expected to add two lines, not to copy a
   block of settings that then drifts from the ones every other node uses.

   The identity - the address and the group key - is deliberately untouched: it
   is what puts the node on the mesh, and a reset that wiped it would take a
   node nobody can reach off the air for good. The broadcast window is left
   empty too: what a node has to say is the board's business, and a tracker
   overrides it right after calling this. */
void loraMeshResetParameters() {
  setAndSaveParameter(PARAM_LORA_ROLE, LORA_ROLE_ENDPOINT);
  setAndSaveParameter(PARAM_LORA_TTL, 2);
  setAndSaveParameter(PARAM_LORA_SPREADING_FACTOR, 9);
  setAndSaveParameter(PARAM_LORA_FREQUENCY, 18736);  // 868.4 MHz
  setAndSaveParameter(PARAM_LORA_BANDWIDTH, 125);
  setAndSaveParameter(PARAM_LORA_HELLO_SECONDS, LORA_HELLO_SECONDS_DEFAULT);

  setAndSaveParameter(PARAM_LORA_INTERVAL_SECONDS, 0);
  setAndSaveParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER, 0);
  setAndSaveParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS, 0);
}
#endif
