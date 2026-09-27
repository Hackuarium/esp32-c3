#include "config.h"
#ifdef THR_LORA_MESH
#include <string.h>

#include "lora/loraHello.h"
#include "lora/loraMesh.h"
#include "params.h"

/* Degrees x 1e6, like the fix in G to J. Not parameters: a position is where
   this board stands, not a setting a block copy should hand to its neighbours,
   and two int32 would take four slots the mesh block does not have. */
#define LOCATION_LATITUDE_KEY "mesh.lat"
#define LOCATION_LONGITUDE_KEY "mesh.lon"

/* the line travels back over (ar) in one console reply, CR and LF included */
#if LORA_HELLO_LOCATION_MAX_LENGTH + 2 > LORA_CONSOLE_MAX_REPLY
#error "The location line no longer fits in one console reply"
#endif

#ifdef THR_GPS
boolean gpsHasCurrentFix();
#endif

/* What a HELLO would say right now. A current fix wins, because it is where
   the node is rather than where somebody said it was; then the position placed
   by hand with (al); otherwise none. The fix is only taken while
   gpsHasCurrentFix() vouches for it - the coordinates in G to J keep their
   last value for ever once the receiver stops solving - and only when it is a
   coordinate at all: until taskGPS first writes them, those slots can hold two
   ERROR_VALUE halves.

   An untouched NVS key reads 0, not an error, so 0,0 is what "not set" looks
   like - which is why (al) forgets 0,0 rather than storing it. */
static void resolveHello(LoraHello* hello) {
  memset(hello, 0, sizeof(LoraHello));
  hello->repeater = getParameter(PARAM_LORA_ROLE) == LORA_ROLE_REPEATER;
  hello->bridge = getParameter(PARAM_LORA_ROLE) == LORA_ROLE_BRIDGE;
#ifdef THR_DRONE_ID
  hello->droneWatcher = true;
#endif
#ifdef THR_GPS
  if (gpsHasCurrentFix()) {
    int32_t latitude = getParameterInt32(PARAM_GPS_LATITUDE);
    int32_t longitude = getParameterInt32(PARAM_GPS_LONGITUDE);
    if (loraHelloIsValidPosition(latitude, longitude)) {
      hello->located = true;
      hello->gps = true;
      hello->latitude = latitude;
      hello->longitude = longitude;
      return;
    }
  }
#endif
  int32_t latitude = getNVSParameterInt32(LOCATION_LATITUDE_KEY);
  int32_t longitude = getNVSParameterInt32(LOCATION_LONGITUDE_KEY);
  if ((latitude != 0 || longitude != 0) &&
      loraHelloIsValidPosition(latitude, longitude)) {
    hello->located = true;
    hello->latitude = latitude;
    hello->longitude = longitude;
  }
}

uint8_t loraMeshHelloBody(uint8_t* body) {
  LoraHello hello;
  resolveHello(&hello);
  return (uint8_t)loraHelloEncode(&hello, body, LORA_HELLO_MAX_SIZE);
}

void loraMeshPrintLocation(Print* output) {
  LoraHello hello;
  resolveHello(&hello);
  char line[LORA_HELLO_LOCATION_MAX_LENGTH + 1];
  loraHelloFormatLocation(&hello, line, sizeof(line));
  output->println(line);
}

/* (al) - where this node's HELLO places it. al46.5191,6.5668 places it by
   hand, al0 forgets that, al alone prints the line.

   Placing a node broadcasts a HELLO straight away: the next automatic one can
   be three hours off, and whoever typed the position is usually standing at
   the post waiting to see it land on the map. It only reaches a bridge that
   hears this node directly - a HELLO is never relayed - so a relay further out
   is read with ar<address>:al instead, whose reply is this same line. */
void processLoraMeshLocationCommand(char* paramValue, Print* output) {
  if (paramValue[0] == '\0') {
    loraMeshPrintLocation(output);
    return;
  }
  int32_t latitude = 0;
  int32_t longitude = 0;
  if (strcmp(paramValue, "0") != 0 &&
      !loraHelloParseLocation(paramValue, &latitude, &longitude)) {
    output->println(
        F("Use al<latitude>,<longitude>, e.g. al46.5191,6.5668, or al0 to "
          "forget it"));
    return;
  }
  if (latitude == 0 && longitude == 0) {
    deleteParameter(LOCATION_LATITUDE_KEY);
    deleteParameter(LOCATION_LONGITUDE_KEY);
    loraMeshPrintLocation(output);
    return;
  }

  setNVSParameterInt32(LOCATION_LATITUDE_KEY, latitude);
  setNVSParameterInt32(LOCATION_LONGITUDE_KEY, longitude);
  loraMeshPrintLocation(output);
  uint8_t body[LORA_HELLO_MAX_SIZE];
  uint8_t length = loraMeshHelloBody(body);
  loraMeshSend(LORA_ADDRESS_BROADCAST, LORA_TYPE_HELLO, body, length, output);
}
#endif
