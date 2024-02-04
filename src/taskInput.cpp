#include "./config.h"

#ifdef PARAM_INPUT_1

#include "./params.h"

void TaskInput(void* pvParameters) {
  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  while (true) {
    vTaskDelay(40);  // 25 times per seconds
    setParameter(PARAM_INPUT_1, digitalRead(10));
    setParameter(PARAM_INPUT_2, digitalRead(9));
    //   Serial.print(getParameter(PARAM_INPUT_1));
    //  Serial.print(" ");
    // Serial.println(getParameter(PARAM_INPUT_2));
  }
}

void taskInput() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskInput, "TaskInput",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif