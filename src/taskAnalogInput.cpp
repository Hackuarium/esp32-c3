#include "./config.h"

#include "./params.h"

#ifdef ANALOG_INPUTS
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
    vTaskDelay(ANALOG_SLEEP);
    for (const auto& input : analogInputs) {
      int16_t value = analogReadMilliVolts(input.pin);
      setParameter(input.parameter, value * input.factor);
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

#endif