#include "config.h"
#ifdef THR_LORA_MESH
#include "lora/loraBridge.h"
#include "params.h"

/* Discards everything written to it. */
class SilentPrint : public Print {
 public:
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t* buffer, size_t size) override {
    (void)buffer;
    return size;
  }
};

static SilentPrint silentPrint;

boolean loraMeshIsBridge() {
  return getParameter(PARAM_LORA_ROLE) == LORA_ROLE_BRIDGE;
}

Print* loraMeshSilent() {
  return &silentPrint;
}

Print* loraBridgeBegin(const char* event) {
  if (!loraMeshIsBridge()) {
    return NULL;
  }
  Serial.print(F("{\"event\":\""));
  Serial.print(event);
  Serial.print('"');
  /* the event member is always first, so every writer below opens with a comma
     and no state has to be carried between them */
  return &Serial;
}

void loraBridgeEnd(Print* output) {
  if (output == NULL) {
    return;
  }
  output->println('}');
}

static void writeKey(Print* output, const char* key) {
  output->print(F(",\""));
  output->print(key);
  output->print(F("\":"));
}

void loraBridgeInt(Print* output, const char* key, int32_t value) {
  if (output == NULL) {
    return;
  }
  writeKey(output, key);
  output->print(value);
}

void loraBridgeFloat(Print* output,
                     const char* key,
                     double value,
                     uint8_t decimals) {
  if (output == NULL) {
    return;
  }
  writeKey(output, key);
  output->print(value, decimals);
}

void loraBridgeText(Print* output,
                    const char* key,
                    const __FlashStringHelper* value) {
  if (output == NULL) {
    return;
  }
  writeKey(output, key);
  output->print('"');
  output->print(value);
  output->print('"');
}

void loraBridgeTextBytes(Print* output,
                         const char* key,
                         const char* value,
                         uint8_t length) {
  if (output == NULL) {
    return;
  }
  writeKey(output, key);
  output->print('"');
  for (uint8_t i = 0; i < length; i++) {
    /* unsigned on purpose: char is signed on xtensa, and a byte past 0x7F would
       then compare as a control character and be escaped from a negative value */
    uint8_t character = (uint8_t)value[i];
    if (character == '"' || character == '\\') {
      output->print('\\');
      output->print((char)character);
    } else if (character == '\n') {
      output->print(F("\\n"));
    } else if (character == '\t') {
      output->print(F("\\t"));
    } else if (character < 0x20 || character >= 0x7F) {
      /* the mesh only ever carries printable text here, so this is the
         belt-and-braces branch: escaped rather than dropped, because a byte
         that should not exist is worth seeing on the host */
      output->print(F("\\u00"));
      output->print(character >> 4, HEX);
      output->print(character & 0x0F, HEX);
    } else {
      output->print((char)character);
    }
  }
  output->print('"');
}

Print* loraBridgeCopy(Print* output) {
  if (!loraMeshIsBridge() || output == &Serial) {
    return NULL;
  }
  return &Serial;
}
#endif
