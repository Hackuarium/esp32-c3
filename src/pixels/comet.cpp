#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "./pixels.h"
#include "params.h"

unsigned int cometCounter = 0;

// We will store in state if it is the head of the comet
void updateComet(Adafruit_NeoPixel& pixels, uint16_t state[]) {
  cometCounter++;
  if ((cometCounter % (21 - getParameter(PARAM_SPEED))) == 0) {
    // we move the head of the comet
    for (int16_t row = 0; row < getParameter(PARAM_NB_ROWS); row++) {
      for (int16_t column = getParameter(PARAM_NB_COLUMNS) - 1; column >= 0;
           column--) {
        uint16_t led = getLedIndex(row, column);
        if ((state[led] == 1) ||  // left to right
            (state[led] == 4)     // bottom to top
        ) {                       // need to turn on the next one full power
          int16_t nextLedIndex = getNextLedIndex(row, column, state[led]);
          if (nextLedIndex >= 0) {
            state[nextLedIndex] = state[led];
            copyPixelColor(pixels, led, nextLedIndex);
          }
          state[led] = 5;
        }
      }
    }

    for (int16_t row = getParameter(PARAM_NB_ROWS) - 1; row >= 0; row--) {
      for (uint16_t column = 0; column < getParameter(PARAM_NB_COLUMNS);
           column++) {
        uint16_t led = getLedIndex(row, column);
        if ((state[led] == 2) ||
            (state[led] == 3)) {  // need to turn on the next one full power
          int16_t nextLedIndex = getNextLedIndex(row, column, state[led]);
          if (nextLedIndex >= 0) {
            state[nextLedIndex] = state[led];
            copyPixelColor(pixels, led, nextLedIndex);
          }
          state[led] = 5;
        }
      }
    }

    // we decrease the intensity of the comet
    for (uint16_t led = 0; led < MAX_LED; led++) {
      if (state[led] == 5) {
        if (decreaseColor(pixels, led)) {
          state[led] = 0;
        }
      }

      if (state[led] == 0) {
        pixels.setPixelColor(led, getBackgroundColor());
      };
    }
  }

  if (random(pow(2, 7 - getParameter(PARAM_INTENSITY))) != 0) {
    return;
  }

  // we create a new comet
  uint16_t row = random(0, getParameter(PARAM_NB_ROWS));
  uint16_t column = random(0, getParameter(PARAM_NB_COLUMNS));
  int16_t led;
  uint8_t direction = getDirection();
  switch (direction) {
    case 1:  // from left to right
      led = getLedIndex(row, 0);
      break;
    case 2:  // from right to left
      led = getLedIndex(row, getParameter(PARAM_NB_COLUMNS) - 1);
      break;
    case 3:  // from top to bottom
      led = getLedIndex(0, column);
      break;
    case 4:  // from bottom to top
      led = getLedIndex(getParameter(PARAM_NB_ROWS) - 1, column);
      break;
    default:
      return;
  }
  state[led] = direction;

  setColor(pixels, led);
}

#endif