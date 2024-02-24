#include "config.h"

#ifdef LED_ON_BOARD

void TaskBlink(void* pvParameters) {
  (void)pvParameters;

  pinMode(LED_ON_BOARD, OUTPUT);

  while (true) {
    digitalWrite(LED_ON_BOARD, HIGH);
    vTaskDelay(50);
    digitalWrite(LED_ON_BOARD, LOW);
    vTaskDelay(950);
  }
}

void taskBlink() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskBlink, "TaskBlink",
                          1024,  // Crash if less than 1024 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif