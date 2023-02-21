// #include "./common.h"
// #include "./params.h"
#include <common.h>
#include <params.h>
#include "taskBlink.h"
#include "taskLogger.h"

// void taskBlink();
// void taskSi7021();
void taskSerial();
// void taskNTPD();
// void taskMQTT();
// void taskBluetooth();
// void taskOneWire();
// void taskWifi();
// void taskOTA();
// void taskWire();

// Logger
// void taskLooger();

void setup() {
  Serial.begin(115200);  // only for debug purpose
  // setupParameters();
  taskSerial();
  // taskSi7021();
  // taskOTA();
  // taskWifi();
  // taskMQTT();
  // taskOneWire();
  // taskNTPD();
  // taskWire();
  // taskBluetooth();
  taskBlink();
  taskLogger();
}

void loop() {
  vTaskDelay(30000);
  /**
  esp_sleep_enable_timer_wakeup(15 * 1e6);
  esp_deep_sleep_start();
  **/
}