#include <pwmWrite.h>
#include "./common.h"
#include "./params.h"

void TaskServo(void* pvParameters) {
  vTaskDelay(1000);

  Pwm pwm = Pwm();

  while (true) {
    pwm.writeServo(8, getParameter(PARAM_SERVO1));
    pwm.writeServo(9, getParameter(PARAM_SERVO2));
    pwm.writeServo(10, getParameter(PARAM_SERVO3));
    vTaskDelay(50);
  }
}

void taskServo() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskServo, "TaskServo",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
