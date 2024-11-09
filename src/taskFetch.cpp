
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "esp_http_client.h"
#include "forecast.h"
#include "fronius.h"

/*
  Update the weather and fronius
*/
void TaskFetch(void* pvParameters) {
  static uint16_t counter = 0;
  vTaskDelay(10000);
  while (true) {
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(5000);
    }
#ifdef FRONIUS
    updateFronius();
#endif
    if (counter % 60 == 0) {
      updateForecast();
    }
    counter++;
    vTaskDelay(1000);
  }
}

void taskFetch() {
  // Now set up two tasks to rntpdun independently.
  xTaskCreatePinnedToCore(TaskFetch, "TaskFetch",
                          20000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
