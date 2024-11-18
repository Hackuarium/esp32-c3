#include "config.h"
#if BOARD_TYPE == KIND_PIXELS
#include "params.h"

void deepSleep(int seconds);
void taskBlink();
void taskSerial();
void taskNTPD();
void taskPixels();
void taskOutput();
void taskOneWire();
void taskServo();
void taskTest();
void taskFetch();
void taskUptime();
void taskWifi();
void taskMQTT();
void taskWifiAP();
void taskOTA();
void taskMDNS();
void taskInternalTemperature();
void taskWebserver();

void setupPixels() {
  Serial.begin(115200);  // only for debug purpose
  setParameter(PARAM_STATUS, 0);

  setupParameters();
  taskPixels();

  taskSerial();

  taskOTA();  // incompatible with Lolin32 ??
              // taskMDNS();  // incompatible with taskOTA !
  taskWifi();
  taskWifiAP();
  taskNTPD();
  taskWebserver();
  taskMQTT();

#if FETCH_FRONIUS == 1 || FETCH_WEATHER == 1
  taskFetch();
#endif

#ifdef PARAM_INT_TEMPERATURE_B
  taskInternalTemperature();
#endif
  // taskUptime();
#ifdef LED_ON_BOARD
  taskBlink();  // can crash tinyexpr if not correctly assigned
#endif

  vTaskDelay(30 * 1000);  // waiting 30s before normal operation
}

void loopPixels() {
  vTaskDelay(100);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  setQualifier(16961);
}

#endif