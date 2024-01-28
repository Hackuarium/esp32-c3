#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "./pixels.h"
#include "font53.h"
#include "params.h"

uint16_t cellularCounter = 0;
uint8_t currentCellularUpdateRow = 0;

void updateCellular(Adafruit_NeoPixel& pixels,
                    uint16_t state[],
                    uint32_t rowColors[],
                    uint8_t programChanged) {
  if (programChanged) {
    rowColors[getParameter(PARAM_NB_ROWS)] = {0};
    state[getLedIndex(0, getParameter(PARAM_NB_COLUMNS) / 2)] = 1;
    setParameter(PARAM_COMMAND_1, getParameter(PARAM_COMMAND_1) % 256);
  }

  // move down all the colors
  for (uint8_t row = getParameter(PARAM_NB_ROWS) - 1; row > 0; row--) {
    rowColors[row] = rowColors[row - 1];
  }
  rowColors[0] = getColor();

  uint8_t nbRows = getParameter(PARAM_NB_ROWS);
  uint8_t nbColumns = getParameter(PARAM_NB_COLUMNS);

  if (currentCellularUpdateRow < (nbRows - 1)) {
    for (uint16_t column = 0; column < nbColumns; column++) {
      uint8_t cellKey =
          (state[getStateIndex(currentCellularUpdateRow, column - 1)] << 2) +
          (state[getStateIndex(currentCellularUpdateRow, column)] << 1) +
          state[getStateIndex(currentCellularUpdateRow, column + 1)];
      uint8_t cellState =
          (getParameter(PARAM_COMMAND_1) & (1 << cellKey)) >> cellKey;
      state[getLedIndex(currentCellularUpdateRow + 1, column)] = cellState;
    }
    currentCellularUpdateRow++;
  }

  // we copy the state to the pixels
  for (uint8_t row = 0; row < nbRows; row++) {
    for (uint16_t column = 0; column < nbColumns; column++) {
      uint16_t led = getLedIndex(row, column);
      if (state[led] == 0 || rowColors[row] == 0) {
        pixels.setPixelColor(led, getBackgroundColor());
      } else {
        pixels.setPixelColor(led, rowColors[row]);
      }
    }
  }

  cellularCounter++;

  if (cellularCounter % (25 * (21 - getParameter(PARAM_SPEED))) == 0) {
    state[getLedIndex(0, getParameter(PARAM_NB_COLUMNS) / 2)] = 1;
    setParameter(PARAM_COMMAND_1, (getParameter(PARAM_COMMAND_1) + 1) % 256);
    currentCellularUpdateRow = 0;
  }
}

#endif