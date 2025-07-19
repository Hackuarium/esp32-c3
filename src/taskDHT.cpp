#include "config.h"
#ifdef DHT22PIN

#include <DHT.h>
#include "params.h"

void TaskDHT22(void* pvParameters) {
  DHT dht(DHT22PIN, DHT22);
  vTaskDelay(100);
  dht.begin();

  (void)pvParameters;
  while (true) {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    /*
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" *C, ");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
    */
    setParameter(PARAM_TEMPERATURE, temperature * 100);
    setParameter(PARAM_HUMIDITY, humidity * 10);
    vTaskDelay(100);
  }
}

void taskDHT22() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskDHT22, "TaskDHT22",
                          2048,  // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif