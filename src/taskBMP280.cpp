#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "./common.h"
#include "./params.h"

void TaskBMP280(void* pvParameters) {
  vTaskDelay(1000);

  Wire.begin(WIRE_SDA, WIRE_SCL);

  Adafruit_BMP280 bmp;  // I2C

  while (!bmp.begin(0x76)) {
    Serial.println(
        F("Could not find a valid BMP280 sensor, check wiring or "
          "try a different address!"));
    while (!bmp.begin(0x76)) {
      vTaskDelay(1000);
    }
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  while (true) {
    vTaskDelay(10);
    if (bmp.takeForcedMeasurement()) {
      setParameter(PARAM_TEMPERATURE, bmp.readTemperature() * 100);
      setParameter(
          PARAM_ALTITUDE,
          round(bmp.readAltitude(1013.25)));  // Adjusted to local forecast!
      vTaskDelay(5);
    }
  }
}

void taskBMP280() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskBMP280, "TaskBMP280",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
