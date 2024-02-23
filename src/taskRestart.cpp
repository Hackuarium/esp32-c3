#include "config.h"
#ifdef PARAM_UPTIME_H

#include "params.h"

void TaskRestart(void* pvParameters) {
  (void)pvParameters;

  while (true) {
    if ((esp_timer_get_time() / (int64_t)1e6) > (3600 * 24)) {
      esp_restart();
    }
  }
}

void taskRestart() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskRestart, "TaskRestart",
                          1024,  // Crash if less than 1024 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif