#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "./common.h"
#include "./getMedianFloat11.h"
#include "./params.h"

void TaskGY521(void* pvParameters) {
  vTaskDelay(1000);

  Wire.begin(WIRE_SDA, WIRE_SCL);

  Adafruit_MPU6050 mpu;

  if (!mpu.begin(104, &Wire)) {
    vTaskDelay(1000);
  }

  float lastAccelerationsX[11] = {0};
  float lastAccelerationsY[11] = {0};
  float lastAccelerationsZ[11] = {0};
  float lastRotationsX[11] = {0};
  float lastRotationsY[11] = {0};
  float lastRotationsZ[11] = {0};
  uint16_t last521Index = 0;

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);

  mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_94_HZ);

  sensors_event_t a, g, temp;
  while (true) {
    vTaskDelay(5);
    mpu.getEvent(&a, &g, &temp);
    last521Index = (last521Index + 1) % 11;

    lastAccelerationsX[last521Index] = a.acceleration.x;
    lastAccelerationsY[last521Index] = a.acceleration.y;
    lastAccelerationsZ[last521Index] = a.acceleration.z;
    lastRotationsX[last521Index] = g.gyro.x;
    lastRotationsY[last521Index] = g.gyro.y;
    lastRotationsZ[last521Index] = g.gyro.z;

    setParameter(PARAM_ACCELERATION_X,
                 getMedianFloat11(lastAccelerationsX) * 100);
    setParameter(PARAM_ACCELERATION_Y,
                 getMedianFloat11(lastAccelerationsY) * 100);
    setParameter(PARAM_ACCELERATION_Z,
                 getMedianFloat11(lastAccelerationsZ) * 100);
    setParameter(PARAM_ROTATION_X, getMedianFloat11(lastRotationsX) * 100);
    setParameter(PARAM_ROTATION_Y, getMedianFloat11(lastRotationsY) * 100);
    setParameter(PARAM_ROTATION_Z, getMedianFloat11(lastRotationsZ) * 100);
  }
}

void taskGY521() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskGY521, "TaskGY521",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
