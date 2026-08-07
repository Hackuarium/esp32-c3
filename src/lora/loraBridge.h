#ifndef _LORA_BRIDGE_H
#define _LORA_BRIDGE_H

#include <Arduino.h>

/* A bridge is the node whose console is a data feed rather than a log: it emits
   one JSON object per line on Serial, so a host can read the port line by line,
   keep what parses and store it. Everything else on the mesh stays quiet by
   default - an endpoint has no reason to narrate its own traffic to a port
   nobody is reading.

   The writers below all tolerate a NULL stream, and loraBridgeBegin returns
   NULL on a node that is not a bridge, so a call site never tests the role:

     Print* json = loraBridgeBegin("rx");
     loraBridgeInt(json, "src", source);
     loraBridgeEnd(json);
*/
boolean loraMeshIsBridge();

Print* loraBridgeBegin(const char* event);
void loraBridgeEnd(Print* output);

void loraBridgeInt(Print* output, const char* key, int32_t value);
void loraBridgeFloat(Print* output,
                     const char* key,
                     double value,
                     uint8_t decimals);
void loraBridgeText(Print* output,
                    const char* key,
                    const __FlashStringHelper* value);

/* The same, for bytes that came from somewhere else: a console reply is
   whatever another node's Print stream produced, so the quotes, backslashes
   and newlines in it are escaped. Without that one stray quote from a remote
   command turns the line into something the host cannot parse. */
void loraBridgeTextBytes(Print* output,
                         const char* key,
                         const char* value,
                         uint8_t length);

/* A command answered over MQTT or the web page still leaves its JSON copy on
   Serial, so the host watching the port sees every exchange and not only the
   ones it started. Returns the extra stream to write to, or NULL. */
Print* loraBridgeCopy(Print* output);

/* Where the automatic hello and the periodic broadcast report their progress:
   nowhere. A bridge still sees them, because the JSON is emitted by the send
   path itself rather than written to this stream. */
Print* loraMeshSilent();

#endif
