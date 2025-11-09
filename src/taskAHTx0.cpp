#include "config.h"
#ifdef WIRE_SDA
#include <Adafruit_AHTX0.h>
#include <Wire.h>

#include "params.h"

void TaskAHTx0(void* pvParameters) {
  vTaskDelay(150);

  Wire.begin(WIRE_SDA, WIRE_SCL);

  Adafruit_AHTX0 aht;
  sensors_event_t humidity, temperature;

  while (!aht.begin()) {  // could be 0x76 or 0x77
    Serial.println(F("Could not find a valid AHTx0 sensor, check wiring"));
    while (!aht.begin()) {
      vTaskDelay(1000);
    }
  }

  while (true) {
    // we prefer to go relatively quickly and average the result
    vTaskDelay(100);

    if (xSemaphoreTake(xSemaphoreWire, 1) == pdTRUE) {
      aht.getEvent(
          &humidity,
          &temperature);  // populate temp and humidity objects with fresh data

      xSemaphoreGive(xSemaphoreWire);
      setParameter(PARAM_TEMPERATURE, temperature.temperature * 100);
      setParameter(PARAM_HUMIDITY, humidity.relative_humidity * 10);
    }
  }
}

void taskAHTx0() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskAHTx0, "TaskAHTx0",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif