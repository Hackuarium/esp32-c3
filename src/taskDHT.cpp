#include "./params.h"
#include "config.h"
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <DHT_U.h>

void TaskDHT22(void* pvParameters) {
  // DHT sensor is connected to GPIO10
  DHT dht(PARAM_INT_TEMPERATURE, DHT22);
  vTaskDelay(100);
  (void)pvParameters;
  while (true) {
    dht.begin();
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" *C, ");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
    vTaskDelay(10000);
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

