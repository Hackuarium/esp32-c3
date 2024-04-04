#include "./config.h"
#include "./params.h"
#include "driver/touch_pad.h"

#ifdef TOUCH_PAD_NUM2

#define TOUCH_GPIO TOUCH_PAD_NUM2

// example in
// https://github.com/espressif/esp-idf/blob/v5.2.1/examples/peripherals/touch_sensor/touch_sensor_v2/touch_pad_read/main/tp_read_main.c

void TaskTouch(void* pvParameters) {
  vTaskDelay(1000);
  touch_pad_init();
  touch_pad_config(TOUCH_GPIO);
  uint32_t touchValue;
  uint32_t previousValue;

  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  touch_pad_fsm_start();

  while (true) {
    vTaskDelay(100);
    touch_pad_read_raw_data(TOUCH_GPIO, &touchValue);  // read raw data.
    long difference = (long)touchValue - (long)previousValue;
    if (abs(difference) > 50) {
      Serial.println(difference);
    }
    previousValue = touchValue;

    //  Serial.print(" ");
    // Serial.println(getParameter(PARAM_INPUT_2));
  }
}

void taskTouch() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskTouch, "TaskTouch",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif