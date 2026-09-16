#ifndef _DRONE_ID_QUEUE_H
#define _DRONE_ID_QUEUE_H

#include <Arduino.h>

#include "droneIdFrames.h"

/* What the two radios hand over, and the ring they hand it over through.

   Neither radio calls back from a place where anything may be printed. The
   Wi-Fi sniffer's callback runs inside the driver task, where a hundred bytes
   at 115200 baud is nine milliseconds of dropped frames; the Bluetooth one
   runs on the NimBLE host task, whose stack is 4 kB and is not the place to
   format a float. So a callback filters, copies the bytes it wants, and
   returns - and the drone task decodes, aggregates and prints. */

#define DRONE_ADDRESS_LENGTH 6

/* Which radio and which framing carried it. Kept per aircraft because it is
   the one thing about a sighting the message body cannot say, and because the
   same aircraft on Bluetooth and on Wi-Fi is two transmitters with two
   addresses - see droneIdTable.h. */
#define DRONE_SOURCE_BLE_LEGACY 0
#define DRONE_SOURCE_BLE_EXTENDED 1
#define DRONE_SOURCE_WIFI_BEACON 2
#define DRONE_SOURCE_WIFI_NAN 3

typedef struct {
  uint8_t address[DRONE_ADDRESS_LENGTH];
  uint8_t source;
  int8_t rssi;
  /* the Wi-Fi channel it arrived on, 0 for Bluetooth */
  uint8_t channel;
  uint8_t length;
  uint8_t payload[DRONE_MAX_PAYLOAD];
} DroneCapture;

void droneIdQueueBegin();

/* Called from a radio callback. Never blocks and never waits for the lock: a
   frame that cannot be stored right now is dropped, because holding up the
   Wi-Fi driver task costs more frames than the one being written. Returns
   false when the frame was dropped, which is worth counting. */
boolean droneIdQueuePush(const DroneCapture* capture);

/* Called from the drone task. False when the ring is empty. */
boolean droneIdQueuePop(DroneCapture* capture);

/* How many frames the ring has had to drop since boot: the honest measure of
   whether the decoder is keeping up with the air. */
uint32_t droneIdQueueDropped();

#endif
