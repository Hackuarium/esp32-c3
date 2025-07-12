#include "./config.h"

#include "./params.h"

struct AnalogInput {
  uint8_t pin;
  int16_t parameter;
  float factor;
};

static const AnalogInput analogInputs[] = {ANALOG_INPUTS};

void TaskAnalogInput(void* pvParameters) {
  for (const auto& input : analogInputs) {
    // maybe this is only valid on ESP32-S3
    analogSetPinAttenuation(input.pin, ADC_11db);
  }

  while (true) {
    vTaskDelay(40);  // 25 times per seconds
    for (const auto& input : analogInputs) {
      int16_t value = analogRead(input.pin);
      setParameter(input.parameter, value * input.factor / 4096 * 3100);
    }
  }
}

void taskAnalogInput() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskAnalogInput, "TaskAnalogInput",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
