#include <FastLED.h>
#include "./common.h"
#include "./params.h"
#include "./pixels.h"

unsigned int waveCounter = 0;

void updateWave(CRGB pixels[]) {
  if ((waveCounter++ % (21 - getParameter(PARAM_SPEED))) != 0)
    return;
  CRGB nextColor = getColor();
  uint8_t direction = getDirection();

  switch (direction) {
    case 1:
      for (uint8_t row = 0; row < getParameter(PARAM_NB_ROWS); row++) {
        uint16_t led = 0;
        for (int16_t column = getParameter(PARAM_NB_COLUMNS) - 1; column >= 0;
             column--) {
          led = getLedIndex(row, column);
          int16_t nextLedIndex = getNextLedIndex(row, column, direction);
          if (nextLedIndex >= 0) {
            pixels[nextLedIndex] = pixels[led];
          }
        }
        pixels[led] = nextColor;
      }
      break;
    case 2:
      for (uint8_t row = 0; row < getParameter(PARAM_NB_ROWS); row++) {
        uint16_t led = 0;
        for (uint8_t column = 0; column < getParameter(PARAM_NB_COLUMNS);
             column++) {
          led = getLedIndex(row, column);
          int16_t nextLedIndex = getNextLedIndex(row, column, direction);
          if (nextLedIndex >= 0) {
            pixels[nextLedIndex] = pixels[led];
          }
        }
        pixels[led] = nextColor;
      }
      break;
    case 3:
      for (uint8_t column = 0; column < getParameter(PARAM_NB_COLUMNS);
           column++) {
        uint16_t led = 0;
        for (int16_t row = getParameter(PARAM_NB_ROWS) - 1; row >= 0; row--) {
          led = getLedIndex(row, column);
          int16_t nextLedIndex = getNextLedIndex(row, column, direction);
          if (nextLedIndex >= 0) {
            pixels[nextLedIndex] = pixels[led];
          }
        }
        pixels[led] = nextColor;
      }
      break;
    case 4:
      for (uint8_t column = 0; column < getParameter(PARAM_NB_COLUMNS);
           column++) {
        uint16_t led = 0;
        for (uint8_t row = 0; row < getParameter(PARAM_NB_ROWS); row++) {
          led = getLedIndex(row, column);
          int16_t nextLedIndex = getNextLedIndex(row, column, direction);
          if (nextLedIndex >= 0) {
            pixels[nextLedIndex] = pixels[led];
          }
        }
        pixels[led] = nextColor;
      }
      break;
  }
  return;
}