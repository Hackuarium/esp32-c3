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

  updateMapping();

  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 40;
  // Initialise the xLastWakeTime variable with the current time.
  xLastWakeTime = xTaskGetTickCount();

  pixels.begin();

  uint16_t state[MAX_LED] = {0};

  pixels.clear();
  pixels.show();

  // setFunction("sin(t-sqrt((x-3.5)^2+(y-3.5)^2))");

  uint8_t previousProgram = -1;
  uint8_t programChanged = 0;

  uint16_t counter = 0;

  while (true) {
    if (previousProgram != getParameter(PARAM_CURRENT_PROGRAM)) {
      previousProgram = getParameter(PARAM_CURRENT_PROGRAM);
      programChanged = 1;
      state[MAX_LED] = {0};
    } else {
      programChanged = 0;
    }

    doAction();

    // Turn off the device on sunset / sunrise schedule

    if ((!isNight() && getParameterBit(PARAM_SCHEDULE, 0)) ||
        (isNight() && getParameterBit(PARAM_SCHEDULE, 1))) {
      pixels.setBrightness(getParameter(PARAM_BRIGHTNESS));
    } else {
      pixels.setBrightness(0);
    }

    /*
    #define NEO_RGB ((0 << 6) | (0 << 4) | (1 << 2) | (2)) // 6
    #define NEO_RBG ((0 << 6) | (0 << 4) | (2 << 2) | (1)) // 9
    #define NEO_GRB ((1 << 6) | (1 << 4) | (0 << 2) | (2)) // 82
    #define NEO_GBR ((2 << 6) | (2 << 4) | (0 << 2) | (1)) // 161
    #define NEO_BRG ((1 << 6) | (1 << 4) | (2 << 2) | (0)) // 88
    #define NEO_BGR ((2 << 6) | (2 << 4) | (1 << 2) | (0)) // 164
    */

    pixels.updateType(getParameter(PARAM_COLOR_LED_MODEL));

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

void printPixelsHelp(Print* output) {
  output->println(F("(pc) Reset parameters for cube"));
  output->println(F("(ps) Reset parameters for square"));
  output->println(F("(pb) Reset parameters for big christmas tree"));
}

void processPixelsCommand(char command,
                          char* paramValue,
                          Print* output) {  // char and char* ??
  switch (command) {
    case 'c':
      setAndSaveParameter(PARAM_BRIGHTNESS, 127);
      setAndSaveParameter(PARAM_INTENSITY, 2);
      setAndSaveParameter(PARAM_SPEED, 19);
      setAndSaveParameter(PARAM_CURRENT_PROGRAM, 4);
      setAndSaveParameter(PARAM_COLOR_MODEL, 7);
      setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 2);
      setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 0);
      setAndSaveParameter(PARAM_NB_ROWS, 1);
      setAndSaveParameter(PARAM_NB_COLUMNS, 144);
      setAndSaveParameter(PARAM_LAYOUT_MODEL, 0);
      setAndSaveParameter(PARAM_COLOR_LED_MODEL, NEO_GRB);
      setAndSaveParameter(PARAM_COLOR_DECREASE_SPEED, 2);
      setAndSaveParameter(PARAM_DIRECTION, 1);
      setAndSaveParameter(PARAM_LED_RED, 127);
      setAndSaveParameter(PARAM_LED_GREEN, 63);
      setAndSaveParameter(PARAM_LED_BLUE, 0);
      setAndSaveParameter(PARAM_SCHEDULE, 2);  // on during the night
      // Full power in the evening but reduce at 10PM and
      // turn off at 12PM Turn on in the morning at 5AM but not too strong
      setAndSaveParameter(PARAM_SUNSET_OFFSET, -30);
      setAndSaveParameter(PARAM_SUNRISE_OFFSET, 30);
      setAndSaveParameter(PARAM_ACTION_1, 30960);
      setAndSaveParameter(PARAM_ACTION_2, 9320);
      setAndSaveParameter(PARAM_ACTION_3, 0);
      setAndSaveParameter(PARAM_ACTION_4, 8300);

      updateMapping();
      break;
    case 's':
      setAndSaveParameter(PARAM_BRIGHTNESS, 63);
      setAndSaveParameter(PARAM_INTENSITY, 2);
      setAndSaveParameter(PARAM_SPEED, 17);
      setAndSaveParameter(PARAM_CURRENT_PROGRAM, 1);
      setAndSaveParameter(PARAM_COLOR_MODEL, 7);
      setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 3);
      setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 0);
      setAndSaveParameter(PARAM_NB_ROWS, 16);
      setAndSaveParameter(PARAM_NB_COLUMNS, 16);
      setAndSaveParameter(PARAM_LAYOUT_MODEL, 0);
      setAndSaveParameter(PARAM_COLOR_LED_MODEL, NEO_GRB);
      setAndSaveParameter(PARAM_COLOR_DECREASE_SPEED, 2);
      setAndSaveParameter(PARAM_DIRECTION, 1);
      setAndSaveParameter(PARAM_LED_RED, 127);
      setAndSaveParameter(PARAM_LED_GREEN, 63);
      setAndSaveParameter(PARAM_LED_BLUE, 0);
      setAndSaveParameter(PARAM_SCHEDULE, 3);  // always on

      updateMapping();
      break;
    case 'b':

      setAndSaveParameter(PARAM_BRIGHTNESS, 63);
      setAndSaveParameter(PARAM_INTENSITY, 2);
      setAndSaveParameter(PARAM_SPEED, 17);
      setAndSaveParameter(PARAM_CURRENT_PROGRAM, 1);
      setAndSaveParameter(PARAM_COLOR_MODEL, 7);
      setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 3);
      setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 0);
      setAndSaveParameter(PARAM_NB_ROWS, 16);
      setAndSaveParameter(PARAM_NB_COLUMNS, 16);
      setAndSaveParameter(PARAM_LAYOUT_MODEL, 1);
      setAndSaveParameter(PARAM_COLOR_LED_MODEL, NEO_GRB);
      setAndSaveParameter(PARAM_COLOR_DECREASE_SPEED, 2);
      setAndSaveParameter(PARAM_DIRECTION, 1);
      setAndSaveParameter(PARAM_LED_RED, 127);
      setAndSaveParameter(PARAM_LED_GREEN, 63);
      setAndSaveParameter(PARAM_LED_BLUE, 0);
      setAndSaveParameter(PARAM_SCHEDULE, 3);  // always on
      setAndSaveParameter(PARAM_SUNSET_OFFSET, 0);
      setAndSaveParameter(PARAM_SUNRISE_OFFSET, 0);
      setAndSaveParameter(PARAM_ACTION_1,
                          30960);  // at 16h brightness to max (15)
      setAndSaveParameter(PARAM_ACTION_2,
                          11080);  // at 18h brightness to 5
      setAndSaveParameter(PARAM_ACTION_3,
                          3320);               // at 22h brightness to 1
      setAndSaveParameter(PARAM_ACTION_4, 0);  // at 0h brightness to 0
      setAndSaveParameter(PARAM_ACTION_5,
                          2300);  // at 5h brightness to 1
      // at 5h we set brightness to 1      setAndSaveParameter(PARAM_ACTION_6,
      // 10420);
      setAndSaveParameter(PARAM_ACTION_7, 540);  // at 7h we set brightness to 5
      setAndSaveParameter(PARAM_ACTION_8, -1);   // ignore

      updateMapping();
      break;
    default:
      printPixelsHelp(output);
  }
}