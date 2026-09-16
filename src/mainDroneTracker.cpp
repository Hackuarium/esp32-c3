#include "config.h"
#if BOARD_TYPE == KIND_DRONE_TRACKER
#include "params.h"

void taskSerial();
void taskDroneId();
void taskLoraMesh();
void loraMeshResetParameters();

void setupDroneTracker() {
  setupParameters();
  taskSerial();
  taskDroneId();
  taskLoraMesh();
}

void loopDroneTracker() { vTaskDelay(100000); }

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  /* The same five the task writes when it finds the block untouched, so a
     board that has just been reset and one that has never been configured
     watch the same way. */
  setAndSaveParameter(PARAM_DRONE_BLE_SECONDS, DRONE_BLE_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_WIFI_SECONDS, DRONE_WIFI_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_WIFI_CHANNEL, DRONE_WIFI_CHANNEL_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_LOG_SECONDS, DRONE_LOG_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_FORGET_SECONDS, DRONE_FORGET_SECONDS_DEFAULT);

  loraMeshResetParameters();

  setQualifier(DRONE_QUALIFIER);
}

#endif
