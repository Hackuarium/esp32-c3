#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "./params.h"
#include "./pixels.h"

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
