#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "./pixels.h"
#include "params.h"

unsigned int rainCounter = 0;

void updateRain(Adafruit_NeoPixel& pixels, uint16_t state[]) {
  rainCounter++;
  for (int led = 0; led < MAX_LED; led++) {
    if (state[led] == 1) {
      if (decreaseColor(pixels, led)) {
        state[led] = 0;
      }
    }
    if (state[led] == 0) {
      pixels.setPixelColor(led, getBackgroundColor());
    };
  }

  if (rainCounter % ((21 - getParameter(PARAM_SPEED))) != 0) {
    return;
  }

  uint16_t nbEvents = 1;
  if (getParameter(PARAM_INTENSITY) >= 3) {
    nbEvents = pow(4, getParameter(PARAM_INTENSITY) - 3);
  } else {
    if (random(0, pow(2, 2 - getParameter(PARAM_INTENSITY))) != 0) {
      nbEvents = 0;
    }
  }

  uint32_t currentColor = getColor();
  for (uint16_t i = 0; i < nbEvents; i++) {
    int led = random(0, getNbLeds());
    state[led] = 1;
    // we prefer that all the pixels generated at once have the same color
    pixels.setPixelColor(led, currentColor);
    // setColor(pixels, led);
  }
}

#endif