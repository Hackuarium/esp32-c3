#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "./common.h"
#include "./params.h"

void TaskOutput(void* pvParameters) {
  vTaskDelay(1000);

  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(21, OUTPUT);

  while (true) {
    digitalWrite(4, getParameter(PARAM_OUTPUT1));
    digitalWrite(5, getParameter(PARAM_OUTPUT2));
    digitalWrite(21, getParameter(PARAM_OUTPUT3));
    vTaskDelay(10);
  }
}

void taskOutput() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskOutput, "TaskOutput",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
