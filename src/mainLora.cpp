#include "config.h"
#if BOARD_TYPE == KIND_LORA
#include "params.h"

void deepSleep(int seconds);
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
void taskLoraSend();
void taskMQTT();
void taskLora();

void taskMDNS();
void taskInternalTemperature();
void taskWebserver();
void task9Outputs();
void taskInput();
void taskRestart();
void taskTimer();

void setupLora() {
  setupParameters();
  taskSerial();
  /// taskWebserver();
  taskNTPD();

  taskLoraSend();
  taskOTA();
  // taskWifi();
  taskMQTT();
  taskOneWire();
  //  taskWifiAP();
  taskBlink();
}

void loopLora() {
  vTaskDelay(100000);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  setQualifier(16961);
}

#endif