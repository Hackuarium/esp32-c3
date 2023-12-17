#include "config.h"
#include "params.h"

void TaskUptime(void* pvParameters) {
  (void)pvParameters;

  while (true) {
    vTaskDelay(10000);
    setParameter(PARAM_UPTIME_H, esp_timer_get_time() / 1e6 / 3600);
  }
}

void taskUptime() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskUptime, "TaskUptime",
                          1024,  // Crash if less than 1024 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
