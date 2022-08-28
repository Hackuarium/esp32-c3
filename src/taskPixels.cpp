#include <Adafruit_NeoPixel.h>
#include "./common.h"
#include "./params.h"
#include "./pixels/comet.h"
#include "./pixels/doAction.h"
#include "./pixels/firework.h"
#include "./pixels/flame.h"
//#include "./pixels/function.h"
#include "./pixels/life.h"
#include "./pixels/meteo.h"
#include "./pixels/pixels.h"
#include "./pixels/rain.h"
#include "./pixels/rgb.h"
#include "./pixels/snake.h"
#include "./pixels/spiral.h"
#include "./pixels/test.h"
#include "./pixels/wave.h"
#include "isNight.h"

#define PIXELS_PIN 8

Adafruit_NeoPixel pixels(MAX_LED, PIXELS_PIN, NEO_GRB + NEO_KHZ800);

void TaskPixels(void* pvParameters) {
  vTaskDelay(3192);

  setAndSaveParameter(PARAM_BRIGHTNESS, 63);
  setAndSaveParameter(PARAM_INTENSITY, 2);
  setAndSaveParameter(PARAM_SPEED, 17);
  setAndSaveParameter(PARAM_CURRENT_PROGRAM, 1);
  setAndSaveParameter(PARAM_COLOR_MODEL, 7);
  setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 3);
  setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 0);
  setAndSaveParameter(PARAM_NB_ROWS, 1);
  setAndSaveParameter(PARAM_NB_COLUMNS, 256);
  setAndSaveParameter(PARAM_LAYOUT_MODEL, 1);
  setAndSaveParameter(PARAM_COLOR_LED_MODEL, NEO_RGB);
  setAndSaveParameter(PARAM_COLOR_DECREASE_SPEED, 2);
  setAndSaveParameter(PARAM_DIRECTION, 0);
  setAndSaveParameter(PARAM_LED_RED, 127);
  setAndSaveParameter(PARAM_LED_GREEN, 63);
  setAndSaveParameter(PARAM_LED_BLUE, 0);
  setAndSaveParameter(PARAM_SCHEDULE, 3);  // always on

  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 40;
  // Initialise the xLastWakeTime variable with the current time.
  xLastWakeTime = xTaskGetTickCount();

  pixels.begin();

  uint8_t state[MAX_LED] = {0};

  pixels.updateType(getParameter(PARAM_COLOR_LED_MODEL));

  pixels.clear();
  pixels.show();

  // setFunction("sin(t-sqrt((x-3.5)^2+(y-3.5)^2))");

  uint8_t previousProgram = -1;
  uint8_t programChanged = 0;

  uint8_t counter = 0;

  while (true) {
    if (previousProgram != getParameter(PARAM_CURRENT_PROGRAM)) {
      previousProgram = getParameter(PARAM_CURRENT_PROGRAM);
      programChanged = 1;
      state[MAX_LED] = {0};
    } else {
      programChanged = 0;
    }

    // doAction();

    pixels.setBrightness(20);
    // Turn off the device on sunset / sunrise schedule
    /*
    if ((!isNight() && getParameterBit(PARAM_SCHEDULE, 0)) ||
        (isNight() && getParameterBit(PARAM_SCHEDULE, 1))) {
      pixels.setBrightness(getParameter(PARAM_BRIGHTNESS)); // super slow ????
    } else {
      pixels.setBrightness(0);
    }
    */
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    switch (getParameter(PARAM_CURRENT_PROGRAM)) {
        /**
      case 0:
        //  updateFunction(pixels);
        break;
        **/
      case 1:
        updateRain(pixels, state);
        break;
      case 2:
        updateRGB(pixels);
        break;
      case 3:
        updateComet(pixels, state);
        break;
      case 4:
        updateWave(pixels);
        break;
      case 5:
        updateMeteo(pixels);
        break;
      case 6:
        if (programChanged) {
          resetSnake(pixels);
        }
        updateSnake(pixels);
        break;
      case 7:
        if (programChanged) {
          setParameter(PARAM_COMMAND_1, -1);
          setParameter(PARAM_COMMAND_5, 0);
          setParameter(PARAM_COMMAND_6, 0);
        }
        updateLife(pixels);
        break;
      case 8:
        updateFlame(pixels);
        break;
      case 9:
        updateSpiral(pixels, state);
        break;
      case 10:
        updateFirework(pixels, state);
        break;
      case 11:
        updateTest(pixels);
        break;
    }
    pixels.show();  // Send the updated pixel colors to the hardware.
  }
}

void taskPixels() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskPixels, "TaskPixels",
                          10000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
