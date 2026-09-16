#include "config.h"
#ifdef THR_DRONE_ID
#include <string.h>

#include "droneIdQueue.h"

/* Eight slots is about two seconds of the worst case worth watching: an
   aircraft sends its position once a second and everything else more slowly,
   so a field with three drones on it produces a handful of frames a second,
   and the task drains the ring every few milliseconds. The ring exists for the
   burst where a pack arrives on Bluetooth and a beacon on Wi-Fi in the same
   millisecond, not to buffer a backlog: a backlog here would mean positions
   being printed later than they are true. */
#define DRONE_QUEUE_SLOTS 8

static DroneCapture captures[DRONE_QUEUE_SLOTS];
static volatile uint8_t queueHead = 0;
static volatile uint8_t queueTail = 0;
static uint32_t droppedFrames = 0;
static SemaphoreHandle_t queueMutex = NULL;

void droneIdQueueBegin() { queueMutex = xSemaphoreCreateMutex(); }

boolean droneIdQueuePush(const DroneCapture* capture) {
  if (queueMutex == NULL || xSemaphoreTake(queueMutex, 0) != pdTRUE) {
    droppedFrames++;
    return false;
  }
  uint8_t next = (uint8_t)((queueHead + 1) % DRONE_QUEUE_SLOTS);
  boolean stored = next != queueTail;
  if (stored) {
    memcpy(&captures[queueHead], capture, sizeof(DroneCapture));
    queueHead = next;
  } else {
    droppedFrames++;
  }
  xSemaphoreGive(queueMutex);
  return stored;
}

boolean droneIdQueuePop(DroneCapture* capture) {
  if (queueMutex == NULL ||
      xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  boolean got = queueTail != queueHead;
  if (got) {
    memcpy(capture, &captures[queueTail], sizeof(DroneCapture));
    queueTail = (uint8_t)((queueTail + 1) % DRONE_QUEUE_SLOTS);
  }
  xSemaphoreGive(queueMutex);
  return got;
}

uint32_t droneIdQueueDropped() { return droppedFrames; }
#endif
