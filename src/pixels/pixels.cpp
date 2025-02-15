#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "Arduino.h"
#include "inttypes.h"
#include "params.h"
#include "pixels.h"

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
#define COLOR_PURE_BLACK 16
#define COLOR_PURE_RED 17
#define COLOR_PURE_GREEN 18
#define COLOR_PURE_YELLOW 19
#define COLOR_PURE_BLUE 20
#define COLOR_PURE_VIOLET 21
#define COLOR_PURE_CYAN 22
#define COLOR_PURE_WHITE 23

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

uint32_t getColor() {
  return getColor(getParameter(PARAM_COLOR_MODEL), getHSVSpeedChange());
}

uint32_t getColor(uint8_t hsbChangeSpeed) {
  return getColor(getParameter(PARAM_COLOR_MODEL), hsbChangeSpeed);
}

uint32_t getColor(uint8_t colorModel, uint8_t hsbChangeSpeed) {
  return getColor(colorModel, hsbChangeSpeed, 255);
}

uint32_t getBackgroundColor() {
  int16_t brightness = getParameter(PARAM_BACKGROUND_BRIGHTNESS);
  if (brightness < 1) {
    return 0;
  }
  return Adafruit_NeoPixel::Color(
      (uint8_t)(getParameter(PARAM_LED_RED) * brightness / 255),
      (uint8_t)(getParameter(PARAM_LED_GREEN) * brightness / 255),
      (uint8_t)(getParameter(PARAM_LED_BLUE) * brightness / 255));
}

uint32_t getColor(uint8_t colorModel,
                  uint8_t hsbChangeSpeed,
                  uint8_t brightness) {
  if (colorModel == COLOR_FIXED) {
    return (getParameter(PARAM_LED_RED) * brightness / 255) << 16 |
           (getParameter(PARAM_LED_GREEN) * brightness / 255) << 8 |
           getParameter(PARAM_LED_BLUE) * brightness / 255;
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
  if (colorModel >= COLOR_PURE_BLACK && colorModel <= COLOR_PURE_WHITE) {
    uint8_t r = (colorModel - COLOR_PURE_BLACK) & 1 ? brightness : 0;
    uint8_t g = (colorModel - COLOR_PURE_BLACK) & 2 ? brightness : 0;
    uint8_t b = (colorModel - COLOR_PURE_BLACK) & 4 ? brightness : 0;
    return Adafruit_NeoPixel::Color(r, g, b);
  }
  // default:
  // if (colorModel == COLOR_FULL_RANDOM) {
  return getHSV360(random(0, 360), 255, brightness);
  //}
}

uint32_t getHSV360(uint16_t h, uint8_t s, uint8_t v) {
  return Adafruit_NeoPixel::ColorHSV(h * (65535 / 360), s, v);
}

HSV rgbToHSV360(uint32_t color) {
  float rf = ((color >> 16) & 255) / 255.0f;
  float gf = ((color >> 8) & 255) / 255.0f;
  float bf = ((color >> 0) & 255) / 255.0f;
  float max = (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
  float min = (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
  float h, s, v = max;
  float d = max - min;
  s = (max == 0) ? 0 : (d / max);

  if (max == min) {
    h = 0;  // achromatic
  } else {
    if (max == rf) {
      h = (gf - bf) / d + (gf < bf ? 6 : 0);
    } else if (max == gf) {
      h = (bf - rf) / d + 2;
    } else {
      h = (rf - gf) / d + 4;
    }
    h *= 60;
  }
  HSV hsv = {(int)(h), (int)(s * 255), (int)(v * 255)};
  return hsv;
}

void setColor(Adafruit_NeoPixel& pixels, uint16_t led) {
  pixels.setPixelColor(led, getColor());
}

void setFullIntensityColor(Adafruit_NeoPixel& pixels, uint16_t led) {
  uint32_t color =
      getColor(getParameter(PARAM_COLOR_MODEL), getHSVSpeedChange(), 255);
  uint8_t r = (color >> 16) & 255;
  uint8_t g = (color >> 8) & 255;
  uint8_t b = (color >> 0) & 255;
  pixels.setPixelColor(led, Adafruit_NeoPixel::Color(r, g, b));
}

void copyPixelColor(Adafruit_NeoPixel& pixels, uint16_t from, uint16_t to) {
  uint8_t* data = pixels.getPixels();
  data[to * 3] = data[from * 3];
  data[to * 3 + 1] = data[from * 3 + 1];
  data[to * 3 + 2] = data[from * 3 + 2];
}

/*
Decrease the intensity in the hsv model to keep the color*/
void decreaseIntensity(Adafruit_NeoPixel& pixels, uint8_t factor) {
  for (uint16_t i = 0; i < pixels.numPixels(); i++) {
    // get color in RGB
    HSV hsv = rgbToHSV360(pixels.getPixelColor(i));
    pixels.setPixelColor(i, getHSV360(hsv.h, hsv.s, (float)hsv.v / factor));
  }
}

bool decreaseColor(Adafruit_NeoPixel& pixels, uint16_t led, uint8_t increment) {
  uint32_t bgColor = getBackgroundColor();
  uint8_t bgR = (bgColor >> 16) & 255;
  uint8_t bgG = (bgColor >> 8) & 255;
  uint8_t bgB = (bgColor >> 0) & 255;

  uint32_t color = pixels.getPixelColor(led);
  uint8_t r = (color >> 16) & 255;
  uint8_t g = (color >> 8) & 255;
  uint8_t b = (color >> 0) & 255;
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
  if (r < bgR) {
    r = bgR;
  }
  if (g < bgG) {
    g = bgG;
  }
  if (b < bgB) {
    b = bgB;
  }
  if (increment > 0) {
    pixels.setPixelColor(led, Adafruit_NeoPixel::Color(r, g, b));
  }
  return r == bgR && g == bgG && b == bgB;
}

bool decreaseColor(Adafruit_NeoPixel& pixels, uint16_t led) {
  uint8_t threshold = 3;
  uint8_t increment = 0;

  int16_t decreaseSpeed = getParameter(PARAM_COLOR_DECREASE_SPEED);
  if (decreaseSpeed >= threshold) {
    increment = pow(2, decreaseSpeed - threshold);
  } else {
    increment = random(0, pow(2, threshold - decreaseSpeed)) == 0;
  }
  return decreaseColor(pixels, led, increment);
}

// we create a matrix that allows to convert row / col to a number
uint16_t mapping[MAX_LED];

/**
 * Returns the real led index based on the mapping
 */
uint16_t getLedIndex(uint16_t led) {
  if (led >= MAX_LED) {
    return 0;
  }
  return mapping[led];
}

uint16_t getLedIndex(uint16_t row, uint16_t column) {
  row = row % getParameter(PARAM_NB_ROWS);
  column = column % getParameter(PARAM_NB_COLUMNS);
  return getLedIndex(row * getParameter(PARAM_NB_COLUMNS) + column);
}

uint16_t getStateIndex(int16_t row, int16_t column) {
  return getLedIndex(row, column);
}

uint16_t getNbLeds() {
  uint16_t nbLeds =
      getParameter(PARAM_NB_COLUMNS) * getParameter(PARAM_NB_ROWS);
  return nbLeds > MAX_LED ? MAX_LED : nbLeds;
}

uint16_t getLedIndex(position_t& position) {
  return getLedIndex(position.row, position.column);
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

int16_t getNextLedIndex(uint16_t row,
                        uint16_t column,
                        uint8_t direction,
                        uint8_t distance) {
  switch (direction) {
    case 1:  // from left to right
      if (column < (getParameter(PARAM_NB_COLUMNS) - distance)) {
        return getLedIndex(row, column + distance);
      }
      return -1;
    case 2:  // from right to left
      if (column > distance - 1) {
        return getLedIndex(row, column - distance);
      }
      return -1;
    case 3:  // from top to bottom
      if (row < (getParameter(PARAM_NB_ROWS) - distance)) {
        return getLedIndex(row + distance, column);
      }
      return -1;
    case 4:  // from bottom to top
      if (row > distance - 1) {
        return getLedIndex(row - distance, column);
      }
      return -1;
  }
  return -1;
}

int16_t getNextLedIndex(uint16_t row, uint16_t column, uint8_t direction) {
  return getNextLedIndex(row, column, direction, 1);
}

/**
 * Generates the mapping
 *
 * (0,0) is on the top left
 */
void updateMapping() {
  uint16_t nbRow = getParameter(PARAM_NB_ROWS);
  uint16_t nbColumn = getParameter(PARAM_NB_COLUMNS);

  if (getParameter(PARAM_LAYOUT_MODEL) ==
      0) {  // normal square horizontal lines starting from top right
    for (uint16_t row = 0; row < nbRow; row++) {
      for (uint16_t column = 0; column < nbColumn; column++) {
        uint16_t led = row * nbColumn + column;
        if (row % 2 == 0) {
          mapping[led] = led;
        } else {
          mapping[led] = row * nbColumn + nbColumn - column - 1;
        }
      }
    }
  } else if (getParameter(PARAM_LAYOUT_MODEL) ==
             1) {  // vertical lines like christmas tree that starts in the
                   // bottom
    for (uint16_t row = 0; row < nbRow; row++) {
      for (uint16_t column = 0; column < nbColumn; column++) {
        uint16_t led = row * nbColumn + column;
        if (row % 2 == 0) {
          mapping[led] = (column + 1) * nbRow - row - 1;
        } else {
          mapping[led] = column * nbRow + row;
        }
      }
    }
    // normal square horizontal lines starting from top left
  } else if (getParameter(PARAM_LAYOUT_MODEL) == 2) {
    for (uint16_t row = 0; row < nbRow; row++) {
      for (uint16_t column = 0; column < nbColumn; column++) {
        uint16_t led = row * nbColumn + column;
        if (row % 2 == 0) {
          mapping[led] = row * nbColumn + nbColumn - column - 1;
        } else {
          mapping[led] = led;
        }
      }
    }

  } else {
    for (uint16_t row = 0; row < nbRow; row++) {
      for (uint16_t column = 0; column < nbColumn; column++) {
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

#endif