#include "./common.h"
#include "./params.h"

void deepSleep(int seconds);

void taskBlink();
void taskSi7021();
void taskSerial();
void taskNTPD();
void taskMQTT();
void taskBluetooth();
void taskOneWire();
void taskWifi();
void taskOTA();
void taskBatteryLevel();
void taskInternalTemperature();
void taskWire();

void setup() {
  Serial.begin(115200);  // only for debug purpose
  setParameter(PARAM_STATUS, 0);

  setupParameters();
  taskSerial();
  taskSi7021();
  taskOTA();
  taskWifi();
  taskBatteryLevel();
  taskInternalTemperature();
  taskMQTT();
  taskOneWire();
  taskNTPD();
  taskWire();
  // taskBluetooth();
  taskBlink();

  vTaskDelay(30 * 1000);  // waiting 30s before normal operation
}

void loop() {
  vTaskDelay(100);

  if (getParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_WIFI)) {
    // wifi not recheable
    deepSleep(getParameter(PARAM_SLEEP_ERROR_DELAY));
  }

  if (getParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_MQTT)) {
    // mqtt not recheable
    deepSleep(getParameter(PARAM_SLEEP_ERROR_DELAY));
  }

  if (getParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_MQTT_PUBLISHED)) {
    // we succeeded to send MQTT info
    deepSleep(getParameter(PARAM_SLEEP_NORMAL_DELAY));
  }
}

void restart() {
  ESP.restart();
}

void deepSleep(int seconds) {
  if (seconds < 10) {
    return;
  }
  esp_sleep_enable_timer_wakeup(seconds * 1e6);
  esp_deep_sleep_start();
  setParameter(PARAM_STATUS, 0);
}
