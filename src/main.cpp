#include "config.h"
#include "params.h"

SemaphoreHandle_t xSemaphoreWire = xSemaphoreCreateBinary();

void setupRocket();
void loopRocket();

void setup() {
  xSemaphoreGive(xSemaphoreWire);
  Serial.begin(115200);  // only for debug purpose

#ifdef ROCKET
  setupRocket();
#else
  setupExample();
#endif

  vTaskDelay(30 * 1000);  // waiting 30s before normal operation
}

void loop() {
#ifdef ROCKET
  loopRocket();
#else
  loopExample();
#endif
  vTaskDelay(100);
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
