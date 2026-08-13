#include "config.h"
#if BOARD_TYPE == KIND_BLE_BEACON
#include "params.h"

void taskSerial();
void taskBLEBeacon();

void setupBleBeacon() {
  setupParameters();
  taskSerial();
  taskBLEBeacon();
}

void loopBleBeacon() {
  vTaskDelay(100000);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  /* the same three taskBLEBeacon writes when it finds the block untouched, so a
     board that has just been reset and one that has never been configured come
     up as the same beacon */
  setAndSaveParameter(PARAM_BLE_TX_POWER, BLE_BEACON_TX_POWER_DEFAULT);
  setAndSaveParameter(PARAM_BLE_ADV_INTERVAL, BLE_BEACON_INTERVAL_DEFAULT_MS);
  setAndSaveParameter(PARAM_BLE_PHY, BLE_BEACON_PHY_DEFAULT);

  setQualifier(16961);
}

#endif
