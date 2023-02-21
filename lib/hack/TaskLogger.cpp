// #define DEBUG_MEMORY    // takes huge amount of memory should not be
// activated !!

/*******************************************************************************
  This thread takes care of the logs and manage the time and its synchronisation
  The thread write the logs at a definite fixed interval of time in the
SST25VF064 chip The time synchronization works through the NTP protocol and our
server
*******************************************************************************/

#include <Logger.h>
#include <params.h>

void TaskLogger(void* pvParameters) {
  (void)pvParameters;

  vTaskDelay(5000);

  Serial.println(F("Go in Logger thread"));
  writeLog(EVENT_ESP32_BOOT, 0);
  vTaskDelay((long)LOG_INTERVAL - (long)(millis() / 1000UL) + 5UL);

  const TickType_t xDelay = LOG_INTERVAL /10 * 1000 / portTICK_PERIOD_MS; // Delay in s

  while (true) {
    Serial.println(F("Save logger"));
    writeLog(0, 0);
    vTaskDelay( xDelay );
  }
}

void taskLogger() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskLogger, "TaskLogger",
                          4096,
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}