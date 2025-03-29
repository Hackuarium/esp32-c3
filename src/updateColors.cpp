#include "./config.h"

#ifdef PARAM_RED1

#include <Arduino.h>
#include "./params.h"
#include "isNight.h"

uint8_t COLORS_OUT1[8] = {
    PARAM_OUT1_COLOR1, PARAM_OUT1_COLOR2, PARAM_OUT1_COLOR3, PARAM_OUT1_COLOR4,
    PARAM_OUT1_COLOR5, PARAM_OUT1_COLOR6, PARAM_OUT1_COLOR7, PARAM_OUT1_COLOR8};

uint8_t COLORS_OUT2[8] = {
    PARAM_OUT2_COLOR1, PARAM_OUT2_COLOR2, PARAM_OUT2_COLOR3, PARAM_OUT2_COLOR4,
    PARAM_OUT2_COLOR5, PARAM_OUT2_COLOR6, PARAM_OUT2_COLOR7, PARAM_OUT2_COLOR8};

uint8_t COLORS_OUT3[8] = {
    PARAM_OUT3_COLOR1, PARAM_OUT3_COLOR2, PARAM_OUT3_COLOR3, PARAM_OUT3_COLOR4,
    PARAM_OUT3_COLOR5, PARAM_OUT3_COLOR6, PARAM_OUT3_COLOR7, PARAM_OUT3_COLOR8};

unsigned int colorCounter = 0;

void updateOutColors(uint8_t colors[8],
                     uint8_t red,
                     uint8_t green,
                     uint8_t blue) {
  double_t floatColor1Index = (double_t)(colorCounter) / 1000;
  uint16_t color1Index = floor(floatColor1Index);
  uint16_t color2Index = color1Index + 1;

  float_t percent1 = 1 - (floatColor1Index - color1Index);
  float_t percent2 = 1 - percent1;

  // we need ot select the 2 right colors
  int16_t color1 = getParameter(colors[color1Index % 8]);
  int16_t color2 = getParameter(colors[color2Index % 8]);

  float_t r1 = (color1 & 0xf00) >> 4;
  float_t g1 = (color1 & 0x0f0) >> 0;
  float_t b1 = (color1 & 0x00f) << 4;
  float_t r2 = (color2 & 0xf00) >> 4;
  float_t g2 = (color2 & 0x0f0) >> 0;
  float_t b2 = (color2 & 0x00f) << 4;

  float_t brightness = 0;
  if ((!isNight() && getParameterBit(PARAM_SCHEDULE, 0)) ||
      (isNight() && getParameterBit(PARAM_SCHEDULE, 1))) {
    brightness = getParameter(PARAM_BRIGHTNESS) / 255.0;
  }

  // we need a smooth transition
  int maxStep =
      getParameter(PARAM_COLOR_TRANSITION_NB_STEPS) > 0
          ? max(1, (int)((float)getParameter(PARAM_BRIGHTNESS) /
                         (float)getParameter(PARAM_COLOR_TRANSITION_NB_STEPS)))
          : 255;

  int newR = (r1 * percent1 + r2 * percent2) * brightness;
  int newG = (g1 * percent1 + g2 * percent2) * brightness;
  int newB = (b1 * percent1 + b2 * percent2) * brightness;

  uint16_t oldR = max(min(getParameter(red), (int16_t)255), (int16_t)0);
  uint16_t oldG = max(min(getParameter(green), (int16_t)255), (int16_t)0);
  uint16_t oldB = max(min(getParameter(blue), (int16_t)255), (int16_t)0);

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

  setParameter(red, newR);
  setParameter(green, newG);
  setParameter(blue, newB);
}

void updateColors() {
  colorCounter += getParameter(PARAM_SPEED);

  updateOutColors(COLORS_OUT1, PARAM_RED1, PARAM_GREEN1, PARAM_BLUE1);

  updateOutColors(COLORS_OUT2, PARAM_RED2, PARAM_GREEN2, PARAM_BLUE2);

  updateOutColors(COLORS_OUT3, PARAM_RED3, PARAM_GREEN3, PARAM_BLUE3);
}

#endif