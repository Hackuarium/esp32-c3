#include "./params.h"
#include "config.h"

SemaphoreHandle_t xSemaphoreWire = xSemaphoreCreateBinary();

void deepSleep(int seconds);
void taskBlink();
void taskSi7021();
void taskGY521();
void taskBMP280();
void taskSerial();
void taskRocket();
void taskNTPD();
void taskDHT22();
void taskMQTT();
void taskPixels();
void taskOutput();
void taskBluetooth();
void taskOneWire();
void taskServo();
void taskSPIFSLogger();
void taskForecast();
void taskUptime();
void taskWifi();
void taskWifiAP();
void taskOTA();
void taskMDNS();
void taskBatteryLevel();
void taskInternalTemperature();
void taskWebserver();
void taskWire();

void setup() {
  xSemaphoreGive(xSemaphoreWire);
  Serial.begin(115200);  // only for debug purpose
  setParameter(PARAM_STATUS, 0);

  setupParameters();
#ifdef THR_PIXELS
  // taskPixels();
#endif
  taskRocket();
  //  taskBMP280();
  // taskGY521();  // = MPU-6050

  taskSerial();
  taskServo();
  // taskSi7021();

  // taskOTA();
  // taskMDNS();  // incompatible with taskOTA
  // taskWifi();
  taskWifiAP();
  taskWebserver();
#ifdef THR_FORECAST
  taskForecast();
#endif

  taskOutput();  // blink 2 3 6

  // taskMQTT();
  // taskOneWire();
  // taskNTPD();
  // taskInternalTemperature();
  taskBatteryLevel();
  // taskUptime();
  taskWire();
  taskSPIFSLogger();
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
