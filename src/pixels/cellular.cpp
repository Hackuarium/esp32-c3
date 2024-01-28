#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "./pixels.h"
#include "font53.h"
#include "params.h"

uint16_t cellularCounter = 0;
uint8_t currentCellularRule = 30;

void resetCellular(uint16_t state[]);

void updateCellular(Adafruit_NeoPixel& pixels, uint16_t state[]) {
  cellularCounter++;
  if (cellularCounter % ((21 - getParameter(PARAM_SPEED)) * 3) != 0) {
    return;
  }
  resetCellular(state);
  if (cellularCounter % 40 == 0) {
    currentCellularRule++;
  }
  uint8_t nbRows = getParameter(PARAM_NB_ROWS);
  uint8_t nbColumns = getParameter(PARAM_NB_COLUMNS);

  for (uint8_t row = 0; row < nbRows - 1; row++) {
    for (uint16_t column = 0; column < nbColumns; column++) {
      uint8_t cellKey = (state[getStateIndex(row, column - 1)] << 2) +
                        (state[getStateIndex(row, column)] << 1) +
                        state[getStateIndex(row, column + 1)];
      uint8_t cellState = (currentCellularRule & (1 << cellKey)) >> cellKey;
      state[getLedIndex(row + 1, column)] = cellState;
    }
  }

  // we copy the state to the pixels
  for (uint8_t row = 0; row < nbRows; row++) {
    for (uint16_t column = 0; column < nbColumns; column++) {
      uint16_t led = getLedIndex(row, column);
      if (state[led] == 0) {
        pixels.setPixelColor(led, getBackgroundColor());
      } else {
        pixels.setPixelColor(led, getColor());
      }
    }
  }
}

void resetCellular(uint16_t state[]) {
  uint8_t nbRows = getParameter(PARAM_NB_ROWS);
  uint8_t nbColumns = getParameter(PARAM_NB_COLUMNS);

  for (uint8_t row = 0; row < nbRows; row++) {
    for (uint16_t column = 0; column < nbColumns; column++) {
      uint16_t led = getLedIndex(row, column);
      state[led] = 0;
    }
  }
  state[getLedIndex(0, getParameter(PARAM_NB_COLUMNS) / 2)] = 1;
}

#endif