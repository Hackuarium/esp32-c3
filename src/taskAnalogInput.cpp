#include "./config.h"

#include "./params.h"

void TaskAnalogInput(void* pvParameters) {
  // code to analog read A07D0 and A1/D1on ESP32-S3

  while (true) {
    vTaskDelay(40);  // 25 times per seconds
    setParameter(PARAM_BATTERY, analogRead(D0));
    setParameter(PARAM_CHARGING, analogRead(D1));
  }
}

void taskAnalogInput() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskAnalogInput, "TaskAnalogInput",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
