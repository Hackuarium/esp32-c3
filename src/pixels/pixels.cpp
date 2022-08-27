#include "pixels.h"
#include "./common.h"
#include "./params.h"
#include "Arduino.h"
#include "FastLED.h"
#include "inttypes.h"

#define HSV_RANDOM 60

#define COLOR_FIXED 0
#define COLOR_ORANGE 1
#define COLOR_ORANGE_RANDOM 2
#define COLOR_GREEN 3
#define COLOR_GREEN_RANDOM 4
#define COLOR_VIOLET 5
#define COLOR_VIOLET_RANDOM 6
#define COLOR_RAINBOW 7
#define COLOR_RAINBOW_RANDOM 8
#define COLOR_FULL_RANDOM 9

uint16_t hsvStatus = 0;

void setCurrentColorModel(uint8_t colorModel) {
  setAndSaveParameter(PARAM_COLOR_MODEL, colorModel);
}

uint8_t getHSVSpeedChange() {
  if (getParameter(PARAM_COLOR_CHANGE_SPEED) >= 3) {
    return pow(2, getParameter(PARAM_COLOR_CHANGE_SPEED) - 3);
  } else {
    return random(0, pow(4, 3 - getParameter(PARAM_COLOR_CHANGE_SPEED))) == 0;
  }
}

// 1 to 7: rgb color
// 8 : one of the 7 colors
// 9 : hsl +- 60
// 10: hsl
// 11: hsl random

CRGB getColor() {
  return getColor(getParameter(PARAM_COLOR_MODEL), getHSVSpeedChange());
}

CRGB getColor(uint8_t hsbChangeSpeed) {
  return getColor(getParameter(PARAM_COLOR_MODEL), hsbChangeSpeed);
}

CRGB getColor(uint8_t colorModel, uint8_t hsbChangeSpeed) {
  return getColor(colorModel, hsbChangeSpeed, 255);
}

CRGB getBackgroundColor() {
  int16_t brightness = getParameter(PARAM_BACKGROUND_BRIGHTNESS);
  if (brightness < 1) {
    return CRGB(0, 0, 0);
  }
  return CRGB(getParameter(PARAM_LED_RED) * brightness / 255,
              getParameter(PARAM_LED_GREEN) * brightness / 255,
              getParameter(PARAM_LED_BLUE) * brightness / 255);
}

CRGB getColor(uint8_t colorModel, uint8_t hsbChangeSpeed, uint8_t brightness) {
  if (colorModel == COLOR_FIXED) {
    return CRGB(getParameter(PARAM_LED_RED) * brightness / 255,
                getParameter(PARAM_LED_GREEN) * brightness / 255,
                getParameter(PARAM_LED_BLUE) * brightness / 255);
  }
  if (colorModel == COLOR_ORANGE) {  // moving red orange yellow
    hsvStatus = (hsvStatus + hsbChangeSpeed) % 120;
    return getHSV360(0 + (hsvStatus < 60 ? hsvStatus : 119 - hsvStatus), 255,
                     brightness);
  }
  if (colorModel == COLOR_ORANGE_RANDOM) {  // random red orange yellow
    return getHSV360(0 + random(0, 60), 255, brightness);
  }
  if (colorModel == COLOR_GREEN) {
    hsvStatus = (hsvStatus + hsbChangeSpeed) % 120;
    return getHSV360(90 + (hsvStatus < 60 ? hsvStatus : 119 - hsvStatus), 255,
                     brightness);
  }
  if (colorModel == COLOR_GREEN_RANDOM) {
    return getHSV360(90 + random(0, 60), 255, brightness);
  }
  if (colorModel == COLOR_VIOLET) {  // moving blue violet
    hsvStatus = (hsvStatus + hsbChangeSpeed) % 240;
    return getHSV360(240 + (hsvStatus < 120 ? hsvStatus : 239 - hsvStatus), 255,
                     brightness);
  }
  if (colorModel == COLOR_VIOLET_RANDOM) {  // random blue violet
    return getHSV360(240 + random(0, 120), 255, brightness);
  }
  if (colorModel == COLOR_RAINBOW_RANDOM) {
    hsvStatus = (hsvStatus + hsbChangeSpeed) % 360;
    uint16_t h = (hsvStatus + random(0, HSV_RANDOM)) % 360;
    return getHSV360(h, 255, brightness);
  }
  if (colorModel == COLOR_RAINBOW) {
    hsvStatus = (hsvStatus + hsbChangeSpeed) % 360;
    return getHSV360(hsvStatus, 255, brightness);
  }
  if (colorModel == COLOR_FULL_RANDOM) {
    return getHSV360(random(0, 360), 255, brightness);
  }
}

CRGB getHSV360(uint16_t h, uint8_t s, uint8_t v) {
  // todo
  CRGB crgb;
  hsv2rgb_rainbow(CHSV((uint8_t)((uint32_t)h * 255 / 360), s, v), crgb);
  return crgb;
}

void setColor(CRGB pixels[], uint16_t led) {
  CRGB color = getColor();
  pixels[led] = color;
}

void setFullIntensityColor(CRGB pixels[], uint16_t led) {
  uint32_t color =
      getColor(getParameter(PARAM_COLOR_MODEL), getHSVSpeedChange(), 255);
  uint8_t r = (color >> 16) & 255;
  uint8_t g = (color >> 8) & 255;
  uint8_t b = (color >> 0) & 255;
  pixels[led] = CRGB(r, g, b);
}

bool decreaseColor(CRGB pixels[], uint16_t led, uint8_t increment) {
  CRGB color = pixels[led];
  uint8_t r = color.r;
  uint8_t g = color.g;
  uint8_t b = color.b;
  if (r > increment) {
    r -= increment;
  } else {
    r = 0;
  }
  if (g > increment) {
    g -= increment;
  } else {
    g = 0;
  }
  if (b > increment) {
    b -= increment;
  } else {
    b = 0;
  }
  pixels[led] = CRGB(r, g, b);
  return r == 0 && g == 0 && b == 0;
}

bool decreaseColor(CRGB pixels[], uint16_t led) {
  uint8_t increment = 0;
  if (getParameter(PARAM_COLOR_DECREASE_SPEED) >= 2) {
    increment = pow(2, getParameter(PARAM_COLOR_DECREASE_SPEED) - 2);
  } else {
    increment =
        random(0, pow(2, 2 - getParameter(PARAM_COLOR_DECREASE_SPEED))) == 0;
  }
  return decreaseColor(pixels, led, increment);
}

// we create a matrix that allows to convert row / col to a number
uint16_t mapping[MAX_LED];

/**
 * Returns the real led index based on the mapping
 */
uint16_t getLedIndex(uint16_t led) {
  return mapping[led];
}

void blank(CRGB pixels[]) {
  for (uint16_t i = 0; i < MAX_LED; i++) {
    pixels[i] = CRGB(0);
  }
}

uint16_t getLedIndex(uint8_t row, uint8_t column) {
  return mapping[row * getParameter(PARAM_NB_COLUMNS) + column];
}

uint16_t getLedIndex(position_t& position) {
  return mapping[position.row * getParameter(PARAM_NB_COLUMNS) +
                 position.column];
}

uint8_t getDirection() {
  switch (getParameter(PARAM_DIRECTION)) {
    case 1:
    case 2:
    case 3:
    case 4:
      return getParameter(PARAM_DIRECTION);
    case 5:
      return random(1, 3);
    case 6:
      return random(3, 5);
    case 7:
      return random(1, 5);
    case 9:  // we extend direction to deal with clockwise
    case 10:
    case 11:
    case 12:
      return (getParameter(PARAM_DIRECTION) & 7) + random(0, 2) * 8;
    case 13:
      return random(1, 3) + random(0, 2) * 8;
    case 14:
      return random(3, 5) + random(0, 2) * 8;
    case 15:
      return random(1, 5) + random(0, 2) * 8;
    default:
      return 0;
  }
}

int16_t getNextLedIndex(uint8_t row, uint8_t column, uint8_t direction) {
  switch (direction) {
    case 1:  // from left to right
      if (column < (getParameter(PARAM_NB_COLUMNS) - 1)) {
        return getLedIndex(row, column + 1);
      }
      return -1;
    case 2:  // from right to left
      if (column > 0) {
        return getLedIndex(row, column - 1);
      }
      return -1;
    case 3:  // from top to bottom
      if (row < (getParameter(PARAM_NB_ROWS) - 1)) {
        return getLedIndex(row + 1, column);
      }
      return -1;
    case 4:  // from bottom to top
      if (row > 0) {
        return getLedIndex(row - 1, column);
      }
      return -1;
  }
  return -1;
}

/**
 * Generates the mapping
 */
void updateMapping() {
  uint8_t nbRow = getParameter(PARAM_NB_ROWS);
  uint8_t nbColumn = getParameter(PARAM_NB_COLUMNS);

  if (getParameter(PARAM_LAYOUT_MODEL) == 0) {
    for (uint8_t row = 0; row < nbRow; row++) {
      for (uint8_t column = 0; column < nbColumn; column++) {
        uint16_t led = row * nbColumn + column;
        if (row % 2 == 0) {
          mapping[led] = led;
        } else {
          mapping[led] = row * nbColumn + nbColumn - column - 1;
        }
      }
    }
  } else {
    for (uint8_t row = 0; row < nbRow; row++) {
      for (uint8_t column = 0; column < nbColumn; column++) {
        uint16_t led = row * nbColumn + column;
        uint16_t squareShift = (row > 7 ? 128 : 0) + (column > 7 ? 64 : 0);
        if (row % 2 == 0) {
          mapping[led] = squareShift + (row % 8) * 8 + (column % 8);
        } else {
          mapping[led] = squareShift + (row % 8) * 8 + 7 - (column % 8);
        }
      }
    }
  }
}
