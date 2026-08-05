#include "config.h"
#ifdef THR_LORA_MESH
#include <stdlib.h>
#include <string.h>

#include "lora/loraBridge.h"
#include "lora/loraMesh.h"
#include "params.h"

/* Parses the "42:" that may prefix a command and leaves text on the rest. An
   empty prefix ("ac:C6") is a deliberate broadcast, so it is not an error. */
static boolean parseDestination(const char** text,
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
static boolean parseValues(const char* text,
                           int16_t* values,
                           uint8_t maximum,
                           uint8_t* count) {
  *count = 0;
  if (*text == '\0') {
    return true;
  }
  while (true) {
    if (*count >= maximum) {
      return false;
    }
    char* end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || value < -32768 || value > 32767) {
      return false;
    }
    values[*count] = (int16_t)value;
    (*count)++;
    text = end;
    if (*text == '\0') {
      return true;
    }
    if (*text != ',') {
      return false;
    }
    text++;
  }
}

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

uint8_t loraMeshApplyCommand(const uint8_t* body, uint8_t bodyLength) {
  if (bodyLength < 3) {
    return LORA_REASON_BAD_BODY;
  }
  uint8_t opcode = body[0];
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
  if (!parseDestination(&text, &destination, output)) {
    return;
  }

  uint8_t firstParameter;
  if (!parseParameterIndex(&text, &firstParameter)) {
    output->println(F("Expected e.g. axA123, ax42:A123 or ax42:A"));
    return;
  }

  int16_t values[LORA_MAX_PARAMETERS_PER_FRAME];
  uint8_t count = 0;
  if (!parseValues(text, values, LORA_MAX_PARAMETERS_PER_FRAME, &count)) {
    output->println(F("Expected e.g. axA123, ax42:A123 or ax42:A"));
    return;
  }

  if (count == 0) {
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

  if ((uint16_t)firstParameter + count > MAX_PARAM) {
    output->println(F("Parameter range out of bounds"));
    return;
  }

  uint8_t body[LORA_MAX_BODY_SIZE];
  uint8_t length =
      encodeValues(firstParameter, values, count, body, LORA_MAX_BODY_SIZE);
  if (length == 0) {
    output->println(F("Too many parameters for one frame"));
    return;
  }

  output->print(F("Setting "));
  output->print(numberToLabel(firstParameter));
  output->print(F(" and "));
  output->print(count - 1);
  output->print(F(" more on "));
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
  if (!parseDestination(&text, &destination, output)) {
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
  if (!parseDestination(&text, &destination, output)) {
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
#endif
