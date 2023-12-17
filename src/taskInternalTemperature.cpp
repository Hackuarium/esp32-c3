#include <SPI.h>
#include "config.h"
#include "./params.h"
#include "driver/temp_sensor.h"

temp_sensor_config_t temp_sensor = {
    .dac_offset = TSENS_DAC_L0,
    .clk_div = 6,
};

void TaskInternalTemperature(void* pvParameters) {
  temp_sensor_set_config(temp_sensor);

  temp_sensor_start();
  float temperature;

  while (true) {
    temp_sensor_read_celsius(&temperature);
    setParameter(PARAM_INT_TEMPERATURE_B, 100 * temperature);
    vTaskDelay(1000);
  }
  temp_sensor_stop();
}

void taskInternalTemperature() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskInternalTemperature, "TaskInternalTemperature",
                          2048,  // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
