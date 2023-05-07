#include <SPIFFS.h>
#include <stdio.h>
#include "./common.h"
#include "./params.h"
#include "esp_log.h"
#include "esp_spiffs.h"

void TaskSPIFSLogger(void* pvParameters) {
  vTaskDelay(1151);

  setParameter(PARAM_LOGGING_INTERVAL, 1);
  setParameter(PARAM_LOGGING_NB_ENTRIES, 0);

  esp_vfs_spiffs_conf_t config = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true,
  };
  esp_vfs_spiffs_register(&config);

  bool fileOpen = false;
  uint16_t loggerCounter = 0;

  TickType_t xLastWakeTime = xTaskGetTickCount();
  TickType_t xFrequency = 0;
  // Initialise the xLastWakeTime variable with the current time.

  FILE* file;
  while (true) {
    if (getParameter(PARAM_LOGGING_INTERVAL) > 0) {
      xFrequency = getParameter(PARAM_LOGGING_INTERVAL) * 1000;
    } else {
      xFrequency = -getParameter(PARAM_LOGGING_INTERVAL);
      if (xFrequency < 10) {
        xFrequency = 10;
      }
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    if (getParameter(PARAM_LOGGING_NB_ENTRIES) == -32768) {
      if (fileOpen) {
        fflush(file);
        fclose(file);
        fileOpen = false;
      }
      setParameter(PARAM_LOGGING_NB_ENTRIES, 0);
      file = fopen("/spiffs/log.txt", "w");
      fprintf(file, "\n");
      fflush(file);
      fclose(file);
    }
    if (getParameter(PARAM_LOGGING_NB_ENTRIES) > 0) {
      if (!fileOpen) {
        file = fopen("/spiffs/log.txt", "a");
        if (file == NULL) {
          continue;
        }
        fileOpen = true;
      }

      setParameter(PARAM_LOGGING_NB_ENTRIES,
                   getParameter(PARAM_LOGGING_NB_ENTRIES) - 1);
      fprintf(file, "%i\t", millis());
      for (int16_t i = 0; i < 16; i++) {
        fprintf(file, "%i\t", getParameter(i));
      }
      fprintf(file, "\n");
      if (getParameter(PARAM_LOGGING_NB_ENTRIES) == 0) {
        fflush(file);
        fclose(file);
        fileOpen = false;
      }
    }
  }
}

void taskSPIFSLogger() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskSPIFSLogger, "TaskSPIFSLogger", 10000,
                          // This stack size can be checked & adjusted
                          // by reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
