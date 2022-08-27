#include <FastLED.h>
#include "./common.h"
#include "./params.h"
#include "./pixels.h"

uint16_t testRow = 0;
uint16_t testColumn = 0;

void updateTest(CRGB pixels[]) {
  testColumn++;
  if (testColumn == getParameter(PARAM_NB_COLUMNS)) {
    testColumn = 0;
    testRow++;
    if (testRow == getParameter(PARAM_NB_ROWS)) {
      testRow = 0;
      blank(pixels);
    }
  }
  uint16_t led = getLedIndex(testRow, testColumn);

  setColor(pixels, led);
}
