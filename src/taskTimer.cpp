#include "./config.h"

#ifdef PARAM_CURRENT_TIMER

#include "./params.h"

void TaskTimer(void* pvParameters) {
  setParameter(PARAM_CURRENT_TIMER, 0);

  int currentTimer = getParameter(PARAM_CURRENT_TIMER);

  while (true) {
    vTaskDelay(40);  // 25 times per seconds
    // how long the light should stay on
    if (getParameter(PARAM_TIMER) == -1) {
      // don't change brightness if not controlled by timer
      continue;
    } else {
      int maxTimer = getParameter(PARAM_TIMER) * 25;
      // did the hand rail was touched?
      if (getParameter(PARAM_INPUT_2) == 0) {
        if (currentTimer < maxTimer - 50 && currentTimer > 100) {
          currentTimer = maxTimer - 50;  // we keep in to max intensity
        } else if (currentTimer < 100) {
          currentTimer = maxTimer;
        }
      }
      if (currentTimer > 0) {
        currentTimer--;
      }
      // we turn on the light in 2s
      if (currentTimer > maxTimer - 50) {
        setParameter(PARAM_BRIGHTNESS, (maxTimer - currentTimer) * 2);
      } else if (currentTimer < 100) {
        setParameter(PARAM_BRIGHTNESS, currentTimer);
      } else {
        setParameter(PARAM_BRIGHTNESS, 100);
      }
      setParameter(PARAM_CURRENT_TIMER, currentTimer);
    }
  }
}

void taskTimer() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskTimer, "TaskTimer",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif