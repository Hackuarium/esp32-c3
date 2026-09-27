#include "loraHello.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MICRODEGREES 1000000l

static void writeInt32(uint8_t* out, int32_t value) {
  /* little-endian, like every int16 value in a parameter body */
  uint32_t bits = (uint32_t)value;
  out[0] = (uint8_t)(bits & 0xFF);
  out[1] = (uint8_t)((bits >> 8) & 0xFF);
  out[2] = (uint8_t)((bits >> 16) & 0xFF);
  out[3] = (uint8_t)(bits >> 24);
}

/* negated in 32 bits, so INT32_MIN has a magnitude too */
static unsigned long magnitude(int32_t value) {
  uint32_t bits = (uint32_t)value;
  return value < 0 ? (unsigned long)(uint32_t)(0u - bits) : bits;
}

/* Degrees as somebody types them: an optional minus sign, digits, and an
   optional fraction. strtod alone would also take a leading space, "nan", an
   exponent and hexadecimal, none of which is a position anybody meant.
   Returns where the number ends, or NULL when there is none. */
static const char* scanDegrees(const char* text, double* degrees) {
  const char* cursor = text;
  if (*cursor == '-') {
    cursor++;
  }
  bool digits = false;
  while (*cursor >= '0' && *cursor <= '9') {
    cursor++;
    digits = true;
  }
  if (*cursor == '.') {
    cursor++;
    while (*cursor >= '0' && *cursor <= '9') {
      cursor++;
      digits = true;
    }
  }
  if (!digits) {
    return NULL;
  }
  *degrees = strtod(text, NULL);
  return cursor;
}

static const char* skipSpaces(const char* text) {
  while (*text == ' ') {
    text++;
  }
  return text;
}

bool loraHelloIsValidPosition(int32_t latitude, int32_t longitude) {
  return latitude >= -90 * MICRODEGREES && latitude <= 90 * MICRODEGREES &&
         longitude >= -180 * MICRODEGREES && longitude <= 180 * MICRODEGREES;
}

size_t loraHelloEncode(const LoraHello* hello, uint8_t* out, size_t outSize) {
  size_t length = hello->located ? LORA_HELLO_MAX_SIZE : LORA_HELLO_MIN_SIZE;
  if (length > outSize) {
    return 0;
  }
  uint8_t flags = 0;
  if (hello->located) {
    flags |= LORA_HELLO_FLAG_POSITION;
    if (hello->gps) {
      flags |= LORA_HELLO_FLAG_GPS;
    }
    writeInt32(out + 1, hello->latitude);
    writeInt32(out + 5, hello->longitude);
  }
  if (hello->droneWatcher) {
    flags |= LORA_HELLO_FLAG_DRONE_WATCHER;
  }
  if (hello->repeater) {
    flags |= LORA_HELLO_FLAG_REPEATER;
  }
  if (hello->bridge) {
    flags |= LORA_HELLO_FLAG_BRIDGE;
  }
  out[0] = flags;
  return length;
}

size_t loraHelloFormatLocation(const LoraHello* hello,
                               char* out,
                               size_t outSize) {
  int written;
  if (!hello->located) {
    written = snprintf(out, outSize, "Location: not set");
  } else {
    unsigned long latitude = magnitude(hello->latitude);
    unsigned long longitude = magnitude(hello->longitude);
    written = snprintf(out, outSize, "Location: %s%lu.%06lu,%s%lu.%06lu (%s)",
                       hello->latitude < 0 ? "-" : "",
                       latitude / MICRODEGREES, latitude % MICRODEGREES,
                       hello->longitude < 0 ? "-" : "",
                       longitude / MICRODEGREES, longitude % MICRODEGREES,
                       hello->gps ? "gps" : "fixed");
  }
  return written < 0 ? 0 : (size_t)written;
}

bool loraHelloParseLocation(const char* text,
                            int32_t* latitude,
                            int32_t* longitude) {
  double latitudeDegrees;
  double longitudeDegrees;
  const char* cursor = scanDegrees(text, &latitudeDegrees);
  if (cursor == NULL) {
    return false;
  }
  /* a map copies a position as "46.5191, 6.5668", so spaces are taken around
     the comma - and nowhere else */
  cursor = skipSpaces(cursor);
  if (*cursor != ',') {
    return false;
  }
  cursor = scanDegrees(skipSpaces(cursor + 1), &longitudeDegrees);
  if (cursor == NULL || *cursor != '\0') {
    return false;
  }
  if (latitudeDegrees < -90 || latitudeDegrees > 90 ||
      longitudeDegrees < -180 || longitudeDegrees > 180) {
    return false;
  }
  *latitude = (int32_t)lround(latitudeDegrees * MICRODEGREES);
  *longitude = (int32_t)lround(longitudeDegrees * MICRODEGREES);
  return true;
}
