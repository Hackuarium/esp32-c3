#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include <freertos/task.h>
#include "./pixels/cellular.h"
#include "./pixels/comet.h"
#include "./pixels/doAction.h"
#include "./pixels/firework.h"
#include "./pixels/flame.h"
#include "./pixels/function.h"
#include "./pixels/gif.h"
#include "./pixels/life.h"
#include "./pixels/line.h"
#include "./pixels/meteo.h"
#include "./pixels/pixels.h"
#include "./pixels/rain.h"
#include "./pixels/rgb.h"
#include "./pixels/snake.h"
#include "./pixels/spiral.h"
#include "./pixels/test.h"
#include "./pixels/wave.h"
#include "forecast.h"
#include "isNight.h"
#include "params.h"
#include "taskNTPD.h"

const uint8_t ACTIONS[8] = {PARAM_ACTION_1, PARAM_ACTION_2, PARAM_ACTION_3,
                            PARAM_ACTION_4, PARAM_ACTION_5, PARAM_ACTION_6,
                            PARAM_ACTION_7, PARAM_ACTION_8};

Adafruit_NeoPixel pixels(MAX_LED, PIXELS_PIN, NEO_GRB + NEO_KHZ800);

void resetLine(int16_t row, int16_t column);
void outdoorSchedule(uint8_t highBrightness, uint8_t lowBrightness);
void clearActions();

void TaskPixels(void* pvParameters) {
  vTaskDelay(7192);

  updateMapping();

  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 40;
  // Initialise the xLastWakeTime variable with the current time.
  xLastWakeTime = xTaskGetTickCount();

  pixels.begin();

  uint16_t state[MAX_LED] = {0};
  uint32_t rowColors[getParameter(PARAM_NB_ROWS)] = {0};

  pixels.clear();
  pixels.show();

  setFunction("sin(t-sqrt((x-3.5)^2+(y-3.5)^2))");
  // setFunction("x + y + t");
  //  setGIF("/gifs/campfire.gif");
  setGIF("/gifs/dog.gif");

  uint8_t previousProgram = -1;
  uint8_t programChanged = 0;
  uint8_t previousBrightness = 0;
  uint8_t currentBrightness = 0;

  uint16_t counter = 0;

  while (true) {
    previousBrightness = currentBrightness;
    if (previousProgram != getParameter(PARAM_CURRENT_PROGRAM)) {
      previousProgram = getParameter(PARAM_CURRENT_PROGRAM);
      programChanged = 1;
      state[MAX_LED] = {0};
      pixels.clear();
    } else {
      programChanged = 0;
    }
    counter++;

    // Turn off the device on sunset / sunrise schedule
    if ((!isNight() && getParameterBit(PARAM_SCHEDULE, 0)) ||
        (isNight() && getParameterBit(PARAM_SCHEDULE, 1))) {
      doAction();
      currentBrightness = getParameter(PARAM_BRIGHTNESS);
    } else {
      currentBrightness = 0;
    }
    if (currentBrightness == 0 && previousBrightness == 0) {
      vTaskDelay(500);
      continue;
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

    // protection against too high values. Need to calculate the amount of mA
    // taking into account the number of LEDs and their color
    uint32_t currentConsumption = 0;  // to have the real value we should
                                      // multiply by 20mA and divide by 256
    for (uint16_t i = 0; i < MAX_LED; i++) {
      uint32_t color = pixels.getPixelColor(i);
      uint8_t r = (color >> 16) & 0xFF;
      uint8_t g = (color >> 8) & 0xFF;
      uint8_t b = color & 0xFF;
      currentConsumption += (r + g + b);
    }
    currentConsumption = (currentConsumption * currentBrightness) >> 16;
    static uint32_t MAX_CURRENT = 10000 / 20;  // 10A
    if (currentConsumption > MAX_CURRENT) {
      // calculate the maximum brightness to avoid too high current
      currentBrightness = (float)currentBrightness * (float)MAX_CURRENT /
                          (float)currentConsumption;
    }
    pixels.setBrightness(currentBrightness);

    pixels.show();  // Send the updated pixel colors to the hardware.

    switch (getParameter(PARAM_CURRENT_PROGRAM)) {
      case 0:
        updateFunction(pixels);
        break;
      case 1:
        updateRain(pixels, state, programChanged);
        break;
      case 2:
        updateRGB(pixels);
        break;
      case 3:
        updateComet(pixels, state, programChanged);
        break;
      case 4:
        updateWave(pixels);
        break;
      case 5:
        updateMeteo(pixels, counter);
        break;
      case 6:
        updateSnake(pixels, programChanged);
        break;
      case 7:
        updateLife(pixels, programChanged);
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
        updateLine(pixels);
        break;
      case 12:
        updateGif(pixels);
        break;
      case 13:
        updateCellular(pixels, state, rowColors, programChanged);
        break;
      case 14:
        updateTest(pixels);
        break;
    }
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
  output->println(F("(pb) Reset parameters for christmas line"));
  output->println(F("(pc) Reset parameters for big christmas tree"));
  output->println(F("(pg) Set gif"));
  output->println(F("(pi) Info"));
  output->println(F("(pl) Reset parameters for line"));
  output->println(F("(pr) Reset parameters for square 8x8"));
  output->println(F("(ps) Reset parameters for square 16x16"));
}

void processPixelsCommand(char command,
                          char* paramValue,
                          Print* output) {  // char and char* ??
  switch (command) {
    case 'b':  // christmass line
      resetLine(1, 1024);
      outdoorSchedule(32, 8);
      setAndSaveParameter(PARAM_BRIGHTNESS, 32);
      setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 32);
      updateMapping();
      break;
    case 'c':  // christmas tree
      resetLine(20, 50);
      outdoorSchedule(32, 8);
      setAndSaveParameter(PARAM_BRIGHTNESS, 32);
      setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 32);
      updateMapping();
      break;
    case 'g':
      setGIF(paramValue);
      break;
    case 'i':  // information
      printTime(output);
      output->print("Brightness: ");
      output->println(getParameter(PARAM_BRIGHTNESS));
      output->print("Background brightness: ");
      output->println(getParameter(PARAM_BACKGROUND_BRIGHTNESS));
      for (uint8_t i = 0; i < sizeof(ACTIONS); i++) {
        int16_t currentActionValue = getParameter(ACTIONS[i]);
        if (currentActionValue == -1) {
          continue;
        }
        int16_t brightness = currentActionValue & 0xFF;
        int16_t quarters = currentActionValue >> 8;
        output->print("Action ");
        output->print(i + 1);
        output->print(" : ");
        output->printf("%02i", quarters / 4);
        output->print(":");
        output->printf("%02i", (quarters % 4) * 15);
        output->print(" - ");
        output->println(brightness);
      }
      output->print("Is night: ");
      output->println(isNight());
      output->print("On during the day: ");
      output->println(getParameterBit(PARAM_SCHEDULE, 0));
      output->print("On during the night: ");
      output->println(getParameterBit(PARAM_SCHEDULE, 1));
      printSunrise(output);
      printSunset(output);
      output->print("Sunset offset: ");
      output->println(getParameter(PARAM_SUNSET_OFFSET));
      output->print("Sunrise offset: ");
      output->println(getParameter(PARAM_SUNRISE_OFFSET));
      break;
    case 'r':  // square 8x8
      setAndSaveParameter(PARAM_BRIGHTNESS, 5);
      setAndSaveParameter(PARAM_INTENSITY, 2);
      setAndSaveParameter(PARAM_SPEED, 17);
      setAndSaveParameter(PARAM_CURRENT_PROGRAM, 1);
      setAndSaveParameter(PARAM_COLOR_MODEL, 7);
      setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 3);
      setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 0);
      setAndSaveParameter(PARAM_NB_ROWS, 8);
      setAndSaveParameter(PARAM_NB_COLUMNS, 8);
      setAndSaveParameter(PARAM_LAYOUT_MODEL, 0);
      setAndSaveParameter(PARAM_COLOR_LED_MODEL, NEO_GRB);
      setAndSaveParameter(PARAM_COLOR_DECREASE_SPEED, 2);
      setAndSaveParameter(PARAM_DIRECTION, 1);
      setAndSaveParameter(PARAM_LED_RED, 127);
      setAndSaveParameter(PARAM_LED_GREEN, 63);
      setAndSaveParameter(PARAM_LED_BLUE, 0);
      setAndSaveParameter(PARAM_SCHEDULE, 3);  // always on
      clearActions();

      updateMapping();
      break;
    case 's':  // square 16x16
      setAndSaveParameter(PARAM_BRIGHTNESS, 5);
      setAndSaveParameter(PARAM_INTENSITY, 2);
      setAndSaveParameter(PARAM_SPEED, 17);
      setAndSaveParameter(PARAM_CURRENT_PROGRAM, 1);
      setAndSaveParameter(PARAM_COLOR_MODEL, 7);
      setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 3);
      setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 0);
      setAndSaveParameter(PARAM_NB_ROWS, 16);
      setAndSaveParameter(PARAM_NB_COLUMNS, 16);
      setAndSaveParameter(PARAM_LAYOUT_MODEL, 2);
      setAndSaveParameter(PARAM_COLOR_LED_MODEL, NEO_GRB);
      setAndSaveParameter(PARAM_COLOR_DECREASE_SPEED, 2);
      setAndSaveParameter(PARAM_DIRECTION, 1);
      setAndSaveParameter(PARAM_LED_RED, 127);
      setAndSaveParameter(PARAM_LED_GREEN, 63);
      setAndSaveParameter(PARAM_LED_BLUE, 0);
      setAndSaveParameter(PARAM_SCHEDULE, 3);  // always on

      setAndSaveParameter(PARAM_NB_PLAYERS, 1);
      setAndSaveParameter(PARAM_WIN_LIMIT, 20);
      clearActions();
      setAndSaveParameter(PARAM_METEO_SLOT_1, SLOT_METEO_WEATHER_ICON);
      setAndSaveParameter(PARAM_METEO_SLOT_2, SLOT_METEO_TEMPERATURE);
      setAndSaveParameter(PARAM_METEO_SLOT_3, SLOT_METEO_TEMPERATURE);
      setAndSaveParameter(PARAM_METEO_SLOT_4, SLOT_METEO_FRONIUS);
      setAndSaveParameter(PARAM_METEO_SLOT_5, SLOT_METEO_TEMPERATURE);
      setAndSaveParameter(PARAM_METEO_SLOT_6, SLOT_METEO_TEMPERATURE);

      updateMapping();
      break;

    case 'l':  // internal line
      resetLine(1, 500);
      setAndSaveParameter(PARAM_COLOR_LED_MODEL,
                          NEO_GRB);  // CA  NEO_RGB: 6 - NEO_GRB: 82
      updateMapping();
      break;
    default:
      printPixelsHelp(output);
  }
}

void outdoorSchedule(uint8_t highBrightness, uint8_t lowBrightness) {
  clearActions();
  setAndSaveParameter(PARAM_SCHEDULE, 2);  // on during the night
  // Full power in the evening but reduce at 10PM and
  // turn off at 12PM Turn on in the morning at 5AM but not too strong
  setAndSaveParameter(PARAM_SUNSET_OFFSET, -30);
  setAndSaveParameter(PARAM_SUNRISE_OFFSET, 30);
  clearActions();
  setAndSaveParameter(
      PARAM_ACTION_1,
      ((4 * 14) << 8) + highBrightness);  // intensity high, at 14:00 but anyway
                                          // we only turn on during the night
  setAndSaveParameter(
      PARAM_ACTION_2,
      ((4 * 22) << 8) + lowBrightness);    // intensity low, at 22:00
  setAndSaveParameter(PARAM_ACTION_3, 0);  // turn off at 00:00
  setAndSaveParameter(
      PARAM_ACTION_4,
      ((4 * 5) << 8) + lowBrightness);  // intensity low, at 5:00
}

void alwaysOn() {
  clearActions();
  setAndSaveParameter(PARAM_SCHEDULE, 3);  // always on
  setAndSaveParameter(PARAM_SUNSET_OFFSET, 0);
  setAndSaveParameter(PARAM_SUNRISE_OFFSET, 0);
}

void resetLine(int16_t row, int16_t column) {
  setAndSaveParameter(PARAM_BRIGHTNESS, 32);
  setAndSaveParameter(PARAM_INTENSITY, 4);
  setAndSaveParameter(PARAM_SPEED, 20);
  setAndSaveParameter(PARAM_CURRENT_PROGRAM, 1);
  setAndSaveParameter(PARAM_COLOR_MODEL, 7);
  setAndSaveParameter(PARAM_COLOR_CHANGE_SPEED, 10);
  setAndSaveParameter(PARAM_BACKGROUND_BRIGHTNESS, 64);
  setAndSaveParameter(PARAM_NB_ROWS, row);
  setAndSaveParameter(PARAM_NB_COLUMNS, column);
  setAndSaveParameter(PARAM_LAYOUT_MODEL, 1);

  setAndSaveParameter(PARAM_COLOR_DECREASE_SPEED, 2);
  setAndSaveParameter(PARAM_DIRECTION, 1);
  setAndSaveParameter(PARAM_LED_RED, 127);
  setAndSaveParameter(PARAM_LED_GREEN, 63);
  setAndSaveParameter(PARAM_LED_BLUE, 0);
  setAndSaveParameter(PARAM_COMMAND_1, 0xf00);
  setAndSaveParameter(PARAM_COMMAND_2, 0xf0f);
  setAndSaveParameter(PARAM_COMMAND_3, 0x20f);
  setAndSaveParameter(PARAM_COMMAND_4, 0x0af);
  setAndSaveParameter(PARAM_COMMAND_5, 0x0fb);
  setAndSaveParameter(PARAM_COMMAND_6, 0x6f0);
  setAndSaveParameter(PARAM_COMMAND_7, 0xfd0);
  setAndSaveParameter(PARAM_COMMAND_8, 0xf70);
  clearActions();
}

void clearActions() {
  setAndSaveParameter(PARAM_ACTION_1, -1);
  setAndSaveParameter(PARAM_ACTION_2, -1);
  setAndSaveParameter(PARAM_ACTION_3, -1);
  setAndSaveParameter(PARAM_ACTION_4, -1);
  setAndSaveParameter(PARAM_ACTION_5, -1);
  setAndSaveParameter(PARAM_ACTION_6, -1);
  setAndSaveParameter(PARAM_ACTION_7, -1);
  setAndSaveParameter(PARAM_ACTION_8, -1);
}

#endif