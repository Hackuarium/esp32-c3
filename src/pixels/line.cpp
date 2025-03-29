#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "params.h"

uint8_t COLORS[8] = {PARAM_COMMAND_1, PARAM_COMMAND_2, PARAM_COMMAND_3,
                     PARAM_COMMAND_4, PARAM_COMMAND_5, PARAM_COMMAND_6,
                     PARAM_COMMAND_7, PARAM_COMMAND_8};

// we will store the color on 15 bits (5 bits per R, G ,B)

unsigned int lineCounter = 0;

uint32_t getPixelColor(uint16_t led, uint32_t currentColor);

void updateLine(Adafruit_NeoPixel& pixels) {
  int16_t speed = getParameter(PARAM_SPEED);
  lineCounter += speed > 10 ? speed * 2 : speed;

  for (uint8_t row = 0; row < getParameter(PARAM_NB_ROWS); row++) {
    for (uint16_t column = 0; column < getParameter(PARAM_NB_COLUMNS);
         column++) {
      uint16_t led = row * getParameter(PARAM_NB_COLUMNS) + column;
      uint32_t currentColor = pixels.getPixelColor(led);
      pixels.setPixelColor(led, getPixelColor(led, currentColor));
    }
  }
}

uint32_t getPixelColor(uint16_t led, uint32_t currentColor) {
  uint16_t pixelWidth = pow(2, getParameter(PARAM_INTENSITY) + 4);
  double_t floatColor1Index = (double_t)(led + lineCounter) / pixelWidth;
  uint16_t color1Index = floor(floatColor1Index);
  uint16_t color2Index = color1Index + 1;

  float_t percent1 = 1 - (floatColor1Index - color1Index);
  float_t percent2 = 1 - percent1;

  // we need ot select the 2 right colors
  int16_t color1 = getParameter(COLORS[color1Index % sizeof(COLORS)]);
  int16_t color2 = getParameter(COLORS[color2Index % sizeof(COLORS)]);

  float_t r1 = (color1 & 0xf00) >> 4;
  float_t g1 = (color1 & 0x0f0) >> 0;
  float_t b1 = (color1 & 0x00f) << 4;
  float_t r2 = (color2 & 0xf00) >> 4;
  float_t g2 = (color2 & 0x0f0) >> 0;
  float_t b2 = (color2 & 0x00f) << 4;

  // how fast can we change the color and what is the previous color ?
  // we need a smooth transition
  int maxStep =
      getParameter(PARAM_COLOR_TRANSITION_NB_STEPS) > 0
          ? max(1, (int)((float)getParameter(PARAM_BRIGHTNESS) /
                         (float)getParameter(PARAM_COLOR_TRANSITION_NB_STEPS)))
          : 255;

  int newR = (r1 * percent1 + r2 * percent2);
  int newG = (g1 * percent1 + g2 * percent2);
  int newB = (b1 * percent1 + b2 * percent2);
  uint8_t oldR = currentColor >> 16 & 0xFF;
  uint8_t oldG = currentColor >> 8 & 0xFF;
  uint8_t oldB = currentColor & 0xFF;

  if (newR > oldR) {
    newR = min(newR, oldR + maxStep);
  } else {
    newR = max(newR, oldR - maxStep);
  }
  if (newG > oldG) {
    newG = min(newG, oldG + maxStep);
  } else {
    newG = max(newG, oldG - maxStep);
  }
  if (newB > oldB) {
    newB = min(newB, oldB + maxStep);
  } else {
    newB = max(newB, oldB - maxStep);
  }

  return (newR << 16) + (newG << 8) + (newB << 0);
}

#endif