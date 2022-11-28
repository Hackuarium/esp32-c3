#include "./common.h"
#include "./params.h"

void deepSleep(int seconds);
void taskBlink();
void taskSi7021();
void taskSerial();
void taskNTPD();
void taskDHT22();
void taskMQTT();
void taskPixels();
void taskBluetooth();
void taskOneWire();
void taskForecast();
void taskUptime();
void taskWifi();
void taskOTA();
void taskMDNS();
void taskBatteryLevel();
void taskInternalTemperature();
void taskWebserver();
void taskWire();

void setup() {
  Serial.begin(115200);  // only for debug purpose
  setParameter(PARAM_STATUS, 0);

  setupParameters();
#ifdef THR_PIXELS
  taskPixels();
#endif
  taskSerial();
  //  taskSi7021();
  taskOTA();
  taskMDNS();  // incompatible with taskOTA
  taskWifi();
  taskWebserver();
#ifdef THR_FORECAST
  taskForecast();
#endif
  // taskMQTT();
  // taskOneWire();
  taskNTPD();
  // taskInternalTemperature();
  // taskBatteryLevel();
  // taskUptime();
  // taskWire();
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
