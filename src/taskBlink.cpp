#include "./common.h"

#define BLINK_GPIO (gpio_num_t)CONFIG_BLINK_GPIO

#ifndef LED_BUILTIN
#define LED_BUILTIN 7
#endif

void TaskBlink(void* pvParameters) {
  (void)pvParameters;

  pinMode(LED_BUILTIN, OUTPUT);

  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    vTaskDelay(2);
    digitalWrite(LED_BUILTIN, LOW);
    vTaskDelay(1000);
  }
}

void blink_task(void *pvParameter)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        /* Blink off (output low) */
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(2 / portTICK_PERIOD_MS);
        // Serial.println("Hello!");
    }
}


void taskBlink() {
  // Now set up two tasks to run independently.
  xTaskCreate(&blink_task, "TaskBlink",
                          configMINIMAL_STACK_SIZE,  // Crash if less than 1024 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          5,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL);
}

// void taskBlink() {
//   // Now set up two tasks to run independently.
//   xTaskCreatePinnedToCore(TaskBlink, "TaskBlink",
//                           1024,  // Crash if less than 1024 !!!!
//                                  // This stack size can be checked & adjusted by
//                                  // reading the Stack Highwater
//                           NULL,
//                           0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
//                               // being the highest, and 0 being the lowest.
//                           NULL, 1);
// }