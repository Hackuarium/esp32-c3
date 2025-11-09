#include "config.h"
#if BOARD_TYPE == KIND_HANDRAIL
#include "params.h"

void gotoSleep(int seconds);
void taskBlink();
void taskSerial();
void taskNTPD();
void taskPixels();
void taskOutput();
void taskOneWire();
void taskTest();
void taskWifi();
void taskWifiAP();
void taskOTA();
void taskMDNS();
void taskInternalTemperature();
void taskWebserver();
void task9Outputs();
void taskInput();
void taskRestart();
void taskTimer();

void setupHandrail() {
  setParameter(PARAM_STATUS, 0);

  setupParameters();
  // taskTest();

  taskSerial();

  taskInput();
  task9Outputs();
  taskRestart();
  taskOTA();
  // taskMDNS();  // incompatible with taskOTA
  // taskWifi();
  taskWifiAP();
  taskWebserver();
  taskTimer();
  taskNTPD();
  taskInternalTemperature();

  vTaskDelay(30 * 1000);  // waiting 30s before normal operation
}

void loopHandrail() {
  vTaskDelay(100);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  setQualifier(16961);
}

#endif