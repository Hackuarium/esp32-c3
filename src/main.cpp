#include "config.h"
#include "params.h"

StaticSemaphore_t xMutexBufferWire;
SemaphoreHandle_t xSemaphoreWire = NULL;

void setupRocket();
void loopRocket();
void setupPixels();
void loopPixels();
void setup9Outputs();
void loop9Outputs();
void setupHandrail();
void loopHandrail();

void setup() {
  xSemaphoreWire = xSemaphoreCreateMutexStatic(&xMutexBufferWire);
  Serial.begin(115200);  // only for debug purpose

#if BOARD_TYPE == KIND_ROCKET
  setupRocket();
#elif BOARD_TYPE == KIND_PIXELS
  setupPixels();
#elif BOARD_TYPE == KIND_9_OUTPUTS
  setup9Outputs();
#elif BOARD_TYPE == KIND_HANDRAIL
  setupHandrail();
#else
  setupExample();
#endif

  vTaskDelay(30 * 1000);  // waiting 30s before normal operation
}

void loop() {
#if BOARD_TYPE == KIND_ROCKET
  loopRocket();
#elif BOARD_TYPE == KIND_PIXELS
  loopPixels();
#elif BOARD_TYPE == KIND_9_OUTPUTS
  loop9Outputs();
#elif BOARD_TYPE == KIND_HANDRAIL
  loopHandrail();
#else
  loopExample();
#endif
  vTaskDelay(100000);
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
