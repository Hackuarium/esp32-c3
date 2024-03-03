#include <WiFi.h>

void sendEventSource(char* event, char* data);

char* tempString = new char[50];

void TaskEventSourceSender(void* pvParameters) {
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(5000);
  }

  while (true) {
    sprintf(tempString, "My current millis is %d", millis());
    sendEventSource("eventLabel", tempString);
    vTaskDelay(1000);
  }
}

void taskEventSourceSender() {
  vTaskDelay(2000);
  // Now set up two tasks to rntpdun independently.
  xTaskCreatePinnedToCore(TaskEventSourceSender, "TaskEventSourceSender",
                          20000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          3,  // CRASH if not priority 3 !!!
                          NULL, 1);
}
