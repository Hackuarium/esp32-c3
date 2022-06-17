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
  taskMQTT();
  taskOneWire();
  taskNTPD();
  taskWire();
  // taskBluetooth();
  taskBlink();
}

void loop() {
  vTaskDelay(1000);

  if (getParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_WIFI)) {
    // wifi not recheable
    deepSleep(600);
  }

  if (getParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_MQTT)) {
    // wifi not recheable
    deepSleep(600);
  }

  if (getParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_MQTT_PUBLISHED)) {
    // we succeeded to send MQTT info
    deepSleep(120);
  }
}

void restart() {
  ESP.restart();
}

void deepSleep(int seconds) {
  esp_sleep_enable_timer_wakeup(seconds * 1e6);
  esp_deep_sleep_start();
  setParameter(PARAM_STATUS, 0);
}
