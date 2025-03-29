#include "./config.h"

#ifdef THR_SCHEDULE

#include "./params.h"
#include "WifiUtilities.h"
#include "doAction.h"
#include "forecast.h"
#include "isNight.h"
#include "printSchedule.h"
#include "taskNTPD.h"

void TaskSchedule(void* pvParameters) {
  while (true) {
    vTaskDelay(40);  // 25 times per seconds

    doAction();
  }
}

void taskSchedule() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskSchedule, "TaskSchedule",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

void printAllScheduleInfo(Print* output) {
  printTime(output);
  output->print("Brightness: ");
  output->println(getParameter(PARAM_BRIGHTNESS));
  printSchedule(output);
  printForecast(output);
}

#endif