#include "config.h"
#if BOARD_TYPE == KIND_DRONE_TRANSMITTER
#include "params.h"

void taskSerial();
void taskDroneTransmitter();

void setupDroneTransmitter() {
  setupParameters();
  taskSerial();
  taskDroneTransmitter();
}

void loopDroneTransmitter() {
  vTaskDelay(100000);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  setAndSaveParameter(PARAM_DRONE_TX_LEGACY, DRONE_TX_LEGACY_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_TX_LONG_RANGE, DRONE_TX_LONG_RANGE_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_TX_CHANNEL, DRONE_TX_CHANNEL_DEFAULT);

  setQualifier(DRONE_TX_QUALIFIER);
}

#endif
