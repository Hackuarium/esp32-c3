#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "./common.h"
#include "./params.h"

void TaskGY521(void* pvParameters) {
  vTaskDelay(1000);

  Wire.begin(WIRE_SDA, WIRE_SCL);

  Adafruit_MPU6050 mpu;

  if (!mpu.begin(104, &Wire)) {
    vTaskDelay(1000);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);

  mpu.setGyroRage(MPU6050_RANGE_2000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  while (true) {
    vTaskDelay(2);
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    setParameter(PARAM_ACCELERATION_X, a.acceleration.x * 100);
    setParameter(PARAM_ACCELERATION_Y, a.acceleration.y * 100);
    setParameter(PARAM_ACCELERATION_Z, a.acceleration.z * 100);
    setParameter(PARAM_ROTATION_X, a.gyro.x * 100);
    setParameter(PARAM_ROTATION_Y, a.gyro.y * 100);
    setParameter(PARAM_ROTATION_Z, a.gyro.z * 100);
  }
}

void taskGY521() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskGY521, "TaskGY521",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
