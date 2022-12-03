#include <Adafruit_NeoPixel.h>
#include "./common.h"
#include "./params.h"
#include "./pixels.h"

unsigned int spiralCounter = 0;

int16_t getSpiralNextLedIndex(uint8_t row, uint8_t column, uint8_t direction);

// We will store in state if it is the head of the spiral
void updateSpiral(Adafruit_NeoPixel& pixels, uint16_t state[]) {
  spiralCounter++;
  if ((spiralCounter % (21 - getParameter(PARAM_SPEED))) == 0) {
    // we move the head of the spiral

    for (uint8_t row = 0; row < getParameter(PARAM_NB_ROWS); row++) {
      for (int16_t column = 0; column < getParameter(PARAM_NB_COLUMNS);
           column++) {
        uint16_t led = getLedIndex(row, column);

        if (((state[led] & 128) == 0) && (state[led] & 7) <= 4 &&
            (state[led] & 7) > 0) {  // need to turn on the next one full power
          int16_t nextLedIndex = getSpiralNextLedIndex(row, column, state[led]);
          if (nextLedIndex >= 0) {
            state[nextLedIndex] = state[led] | 128;
            copyPixelColor(pixels, led, nextLedIndex);
          }
          state[led] =
              5;  // current led becomes 'normal' meaning it may decrease
        }
      }
    }

    // we decrease the intensity of the spiral
    for (uint16_t led = 0; led < MAX_LED; led++) {
      state[led] = state[led] & 127;  // remove in progress
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

  // we create a new spiral
  if (random(pow(2, 7 - getParameter(PARAM_INTENSITY))) != 0) {
    return;
  }

  uint8_t row = random(0, getParameter(PARAM_NB_ROWS));
  uint8_t column = random(0, getParameter(PARAM_NB_COLUMNS));
  int16_t led;
  uint8_t direction = getDirection();
  switch (direction & 7) {
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

int16_t getSpiralNextLedIndex(uint8_t row, uint8_t column, uint8_t direction) {
  uint16_t nbColumns = getParameter(PARAM_NB_COLUMNS);
  uint16_t nbRows = getParameter(PARAM_NB_ROWS);

  int8_t clockwise = (direction & 8) ? 1 : -1;

  switch (direction & 7) {
    case 1:  // from left to right
      if (column < (nbColumns - 1)) {
        return getLedIndex((nbRows + row + clockwise) % nbRows, column + 1);
      }
      return -1;
    case 2:  // from right to left
      if (column > 0) {
        return getLedIndex((nbRows + row - clockwise) % nbRows, column - 1);
      }
      return -1;
    case 3:  // from top to bottom
      if (row < (nbRows - 1)) {
        return getLedIndex(row + 1,
                           (column + clockwise + nbColumns) % nbColumns);
      }
      return -1;
    case 4:  // from bottom to top
      if (row > 0) {
        return getLedIndex(row - 1,
                           (column - clockwise + nbColumns) % nbColumns);
      }
      return -1;
  }
  return -1;
}
