#include <Adafruit_NeoPixel.h>
#include "./common.h"
#include "./params.h"

uint8_t COLORS[8] = {PARAM_COMMAND_1, PARAM_COMMAND_2, PARAM_COMMAND_3,
                     PARAM_COMMAND_4, PARAM_COMMAND_5, PARAM_COMMAND_6,
                     PARAM_COMMAND_7, PARAM_COMMAND_8};

// we will store the color on 15 bits (5 bits per R, G ,B)

unsigned int lineCounter = 0;
uint16_t lineShift = 0;

uint32_t getPixelColor(uint16_t led, uint16_t shift);

void updateLine(Adafruit_NeoPixel& pixels) {
  if ((lineCounter++ % (21 - getParameter(PARAM_SPEED))) != 0)
    return;

  lineShift++;
  if (getParameter(PARAM_SPEED) > 10) {
    lineShift += getParameter(PARAM_SPEED) - 10;
  }

  for (uint8_t row = 0; row < getParameter(PARAM_NB_ROWS); row++) {
    for (uint16_t column = 0; column < getParameter(PARAM_NB_COLUMNS);
         column++) {
      uint16_t led = row * getParameter(PARAM_NB_COLUMNS) + column;
      pixels.setPixelColor(led, getPixelColor(led, lineShift));
    }
  }
}

uint32_t getPixelColor(uint16_t led, uint16_t shift) {
  uint16_t pixelWidth = pow(2, getParameter(PARAM_INTENSITY) + 4);
  uint16_t index = (led + lineShift) % getParameter(PARAM_NB_COLUMNS);
  uint8_t howClose = index % pixelWidth;
  uint8_t color1Index = (index - howClose) / pixelWidth;
  uint8_t color2Index = color1Index + 1;

  // we need ot select the 2 right colors
  int16_t color1 = getParameter(COLORS[color1Index % sizeof(COLORS)]);
  int16_t color2 = getParameter(COLORS[color2Index % sizeof(COLORS)]);
  float_t percent1 = 1 - ((float)howClose / (float)pixelWidth);
  float_t percent2 = 1 - percent1;

  float_t r1 = (color1 & 0xf00) >> 4;
  float_t g1 = (color1 & 0x0f0) >> 0;
  float_t b1 = (color1 & 0x00f) << 4;
  float_t r2 = (color2 & 0xf00) >> 4;
  float_t g2 = (color2 & 0x0f0) >> 0;
  float_t b2 = (color2 & 0x00f) << 4;

  return (((int)(r1 * percent1 + r2 * percent2) << 16)) +
         (((int)(g1 * percent1 + g2 * percent2) << 8)) +
         (((int)(b1 * percent1 + b2 * percent2) << 0));

  // we need to blend those 2 colors
}