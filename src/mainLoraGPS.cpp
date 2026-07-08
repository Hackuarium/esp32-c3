#include "config.h"
#if BOARD_TYPE == KIND_LORA_GPS
#include "params.h"

void taskBlink();
void taskSerial();
void taskGPS();
void taskLoraBeacon();
void taskAnalogInput();
void taskBMP280();
void taskAHTx0();

void setupLoraGPS() {
  setupParameters();
  taskSerial();

  taskGPS();
  taskLoraBeacon();

  // taskAnalogInput();
  // taskBMP280();
  // taskAHTx0();

  taskBlink();
}

void loopLoraGPS() {
  vTaskDelay(100000);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  // The qualifier is used as the unique drone id in the beacon packets.
  setQualifier(7275);
}

#endif
