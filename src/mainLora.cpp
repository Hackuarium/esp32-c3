#include "config.h"
#if BOARD_TYPE == KIND_LORA
#include "params.h"

void gotoSleep(int seconds);
void taskBlink();
void taskSerial();
void taskNTPD();
void taskPixels();
void taskAnalogInput();
void taskOutput();
void taskOneWire();
void taskTest();
void taskWifi();
void taskWifiAP();
void taskOTA();
void taskLoraSend();
void taskMQTT();
void taskDHT22();
void taskBMP280();
void taskAHTx0();

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
  // taskNTPD();

  taskLoraSend();
  // taskOTA();

  taskAnalogInput();
  taskBMP280();
  taskAHTx0();

  // taskWifi();
  // taskMQTT();
  // taskDHT22();
  // taskOneWire();
  // taskWifiAP();
  taskBlink();
}

void loopLora() {
  vTaskDelay(100000);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  setAndSaveParameter(PARAM_LORA_SLEEP_SECONDS, 0);
  setAndSaveParameter(PARAM_LORA_INTERVAL_SECONDS, 10);
  uint32_t devAddr = 0x260B400B;

  setNVSParameterInt32("lora.devAddr", devAddr);

  setBlobParameterFromHex("lora.appSKey", "0349D4C7E08E86ADA421BEAB13930992");
  setBlobParameterFromHex("lora.nwkSEncKey",
                          "629258DE954A3021CCA261CC84EAE453");
  deleteParameter("lora.fNwkSIntK");
  deleteParameter("lora.sNwkSIntK");

  setQualifier(7275);
}

#endif