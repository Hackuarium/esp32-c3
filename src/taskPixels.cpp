#include <FastLED.h>
#include "./common.h"
#include "./params.h"
#include "./pixels/comet.h"
#include "./pixels/pixels.h"

#define PIXELS_PIN 8

void TaskPixels(void* pvParameters) {
  vTaskDelay(3192);

  setAndSaveParameter(PARAM_BRIGHTNESS, 63);
  setAndSaveParameter(PARAM_INTENSITY, 4);
  setAndSaveParameter(PARAM_SPEED, 17);
  setAndSaveParameter(PARAM_CURRENT_PROGRAM, 0);
  setAndSaveParameter(PARAM_COLOR_MODEL, 8);
  setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 3);
  setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 0);
  setAndSaveParameter(PARAM_NB_ROWS, 1);
  setAndSaveParameter(PARAM_NB_COLUMNS, 100);
  setAndSaveParameter(PARAM_LAYOUT_MODEL, 1);
  setAndSaveParameter(PARAM_COLOR_MODEL, 0);

  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 40;
  // Initialise the xLastWakeTime variable with the current time.
  xLastWakeTime = xTaskGetTickCount();
  CRGB pixels[MAX_LED];
  uint8_t state[MAX_LED] = {0};

  switch (getParameter(PARAM_COLOR_LED_MODEL)) {
    case 0:
      LEDS.addLeds<WS2812B, PIXELS_PIN, RGB>(pixels, MAX_LED);
      break;
    case 1:
      LEDS.addLeds<WS2812B, PIXELS_PIN, BRG>(pixels, MAX_LED);
      break;
    case 2:
      LEDS.addLeds<WS2812B, PIXELS_PIN, GRB>(pixels, MAX_LED);
      break;
  }

  blank(pixels);

  LEDS.show();  // Initialize all pixels to 'off'

  uint8_t previousProgram = -1;
  uint8_t programChanged = 0;

  while (true) {
    if (previousProgram != getParameter(PARAM_CURRENT_PROGRAM)) {
      previousProgram = getParameter(PARAM_CURRENT_PROGRAM);
      programChanged = 1;
      state[MAX_LED] = {0};
      programChanged = 0;
    }

    LEDS.setBrightness(getParameter(PARAM_BRIGHTNESS));

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    switch (getParameter(PARAM_CURRENT_PROGRAM)) {
      case 0:
        updateComet(pixels, state);
        break;
    }
    LEDS.show();  // Send the updated pixel colors to the hardware.
  }
}

void taskPixels() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskPixels, "TaskPixels",
                          20000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
