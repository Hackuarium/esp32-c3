#include "config.h"
#if BOARD_TYPE == KIND_AIR_TRAFFIC
#include "params.h"

void taskSerial();
void taskAirTraffic();
#ifdef THR_GPS
void taskGPS();
#endif

void setupAirTraffic() {
  setupParameters();
  taskSerial();
  taskAirTraffic();
#ifdef THR_GPS
  taskGPS();
#endif
}

void loopAirTraffic() { vTaskDelay(100000); }

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  /* The same values the task writes when it finds the block untouched, so a
     board that has just been reset and one never configured listen alike. */
  setAndSaveParameter(PARAM_AIR_PROTOCOLS, AIR_PROTOCOLS_DEFAULT);
  setAndSaveParameter(PARAM_AIR_TURN_MS, AIR_TURN_MS_DEFAULT);
  setAndSaveParameter(PARAM_AIR_CLOCK_OFFSET_MS, AIR_CLOCK_OFFSET_MS_DEFAULT);

  setQualifier(AIR_QUALIFIER);
}

#endif
