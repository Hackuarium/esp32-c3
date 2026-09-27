#ifndef _LORA_HELLO_H
#define _LORA_HELLO_H

#include <stddef.h>
#include <stdint.h>

/* What a HELLO says about the node that sent it.

     flags(1) [latitude(4) longitude(4)]

     flags, bit 0   a position follows
            bit 1   that position is a current GPS fix, clear when it was
                    placed by hand with (al)
            bit 2   this node watches for drones (built with THR_DRONE_ID)
            bit 3   this node relays what it hears (role DA1)
            bit 4   this node is a bridge (role DA2): it feeds a host and
                    receipts what is meant for one
            bits 5-7  0, reserved

   latitude and longitude are int32 little-endian, degrees x 1e6 - the scale of
   PARAM_GPS_LATITUDE and PARAM_GPS_LONGITUDE, so a fix goes out as taskGPS
   stored it.

   A drone watcher on a fence has no GPS, and the host still has to put it on a
   map - and to rebuild the TRACK coordinates it relays, which travel modulo a
   window and are put back against the position of the post that sent them.
   The HELLO is the frame every node sends anyway, so the position rides on it
   rather than costing a frame of its own.

   An empty body is what older firmware sends, and no receiver reads the body,
   so the two coexist on one mesh. Nothing here needs Arduino, so it is tested
   on the host. */

#define LORA_HELLO_FLAG_POSITION 0x01
#define LORA_HELLO_FLAG_GPS 0x02
#define LORA_HELLO_FLAG_DRONE_WATCHER 0x04
#define LORA_HELLO_FLAG_REPEATER 0x08
#define LORA_HELLO_FLAG_BRIDGE 0x10

/* flags alone, and flags followed by a position */
#define LORA_HELLO_MIN_SIZE 1
#define LORA_HELLO_MAX_SIZE 9

/* "Location: -90.000000,-180.000000 (fixed)", the longest line a valid
   position prints. It is what (ar42:al) brings back, and a console reply holds
   43 bytes, line ending included. */
#define LORA_HELLO_LOCATION_MAX_LENGTH 40

typedef struct {
  /* a position follows */
  bool located;
  /* the position is a current GPS fix rather than one placed by hand; ignored
     without a position */
  bool gps;
  bool droneWatcher;
  bool repeater;
  /* Nothing else on the air says it: a host knows the bridge it is plugged
     into from (ai), and any other one - whose receipts stop the repeaters as
     surely as its own bridge's - would otherwise pass for a plain node. */
  bool bridge;
  /* degrees x 1e6 */
  int32_t latitude;
  int32_t longitude;
} LoraHello;

/* -90 to 90 and -180 to 180 degrees, in degrees x 1e6. */
bool loraHelloIsValidPosition(int32_t latitude, int32_t longitude);

/* Writes the body. Returns its length - 1 without a position, 9 with one - or
   0 when it does not fit in outSize. */
size_t loraHelloEncode(const LoraHello* hello, uint8_t* out, size_t outSize);

/* The line (al) and (ai) print, without a line ending:

     Location: 46.519100,6.566800 (fixed)
     Location: 46.519100,6.566800 (gps)
     Location: not set

   The host parses it, so the degrees are always six decimals, printed from
   the integer rather than through a float that could round 0.9999995 up.
   Returns the length snprintf reports. */
size_t loraHelloFormatLocation(const LoraHello* hello,
                               char* out,
                               size_t outSize);

/* Reads "46.5191,6.5668": two decimal degrees and one comma, with optional
   spaces around the comma ("46.5191, 6.5668"), each in range. Rounds to
   degrees x 1e6. Returns false, leaving both untouched, for anything else. */
bool loraHelloParseLocation(const char* text,
                            int32_t* latitude,
                            int32_t* longitude);

#endif
