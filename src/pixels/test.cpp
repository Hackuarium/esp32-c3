#include "config.h"
#ifdef MAX_LED

#include < Adafruit_NeoPixel.h>
#include "./pixels.h"
#include "params.h"

uint16_t testRow = 0;
uint16_t testColumn = 0;

void updateTest(Adafruit_NeoPixel& pixels) {
  testColumn++;
  if (testColumn == getParameter(PARAM_NB_COLUMNS)) {
    testColumn = 0;
    testRow++;
    if (testRow == getParameter(PARAM_NB_ROWS)) {
      testRow = 0;
      pixels.clear();
    }
  }
  uint16_t led = getLedIndex(testRow, testColumn);

  setColor(pixels, led);
}

#endif