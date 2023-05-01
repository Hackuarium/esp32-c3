#include <pwmWrite.h>
#include "./common.h"
#include "./params.h"

void TaskServo(void* pvParameters) {
  vTaskDelay(1000);

  Pwm pwm = Pwm();

  while (true) {
    for (int i = 0; i < 180; i++) {
      pwm.writeServo(8, i);
      vTaskDelay(50);
    }
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
