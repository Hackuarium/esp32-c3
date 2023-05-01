#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "./common.h"
#include "./params.h"

void TaskBlinkAll(void* pvParameters) {
  vTaskDelay(1000);

  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(21, OUTPUT);

  while (true) {
    vTaskDelay(1000);
    digitalWrite(4, HIGH);
    vTaskDelay(1000);
    digitalWrite(5, HIGH);
    vTaskDelay(1000);
    digitalWrite(21, HIGH);
    vTaskDelay(1000);
    digitalWrite(4, LOW);
    vTaskDelay(1000);
    digitalWrite(5, LOW);
    vTaskDelay(1000);
    digitalWrite(21, LOW);
  }
}

void taskBlinkAll() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskBlinkAll, "TaskBlinkAll",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
