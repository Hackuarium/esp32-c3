#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "./common.h"
#include "./getMedianFloat11.h"
#include "./getMedianInt11.h"
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
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,   /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,   /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,  /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,    /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_1); /* Standby time. */

  float lastPressures[11] = {0};
  int16_t lastTemperatures[11] = {0};
  uint16_t last280Index = 0;

  while (true) {
    // we prefer to go relatively quickly and average the result
    vTaskDelay(10);

    last280Index = (last280Index + 1) % 11;

    lastTemperatures[last280Index] = bmp.readTemperature() * 100;
    setParameter(PARAM_TEMPERATURE, getMedianInt11(lastTemperatures));

    float pressure = bmp.readPressure() / 100;
    lastPressures[last280Index] = pressure;
    float medianPressure = getMedianFloat11(lastPressures);
    setParameter(PARAM_PRESSURE, medianPressure * 10);

    float altitude = 44330 * (1.0 - pow(medianPressure / 1013.25, 0.1903));
    if (getParameter(PARAM_ALTITUDE_GROUND) == ERROR_VALUE) {
      setAndSaveParameter(PARAM_ALTITUDE_GROUND, round(altitude));
    }

    setParameter(PARAM_RELATIVE_ALTITUDE,
                 round(altitude) - getParameter(PARAM_ALTITUDE_GROUND));
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
