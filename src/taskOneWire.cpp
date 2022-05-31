#include "./common.h"

void TaskOneWire(void* pvParameters) {
  (void)pvParameters;

  while (true) {
    vTaskDelay(1000);
  }
}

void taskOneWire() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskOneWire, "TaskOneWire",
                          1024,  // Crash if less than 1024 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
