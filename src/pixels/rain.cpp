#include <Adafruit_NeoPixel.h>
#include "./common.h"
#include "./params.h"
#include "./pixels.h"

void updateRain(Adafruit_NeoPixel& pixels, uint16_t state[]) {
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

  uint8_t nbEvents = 1;
  if (getParameter(PARAM_INTENSITY) >= 3) {
    nbEvents = pow(2, getParameter(PARAM_INTENSITY) - 3);
  } else {
    if (random(0, pow(2, 2 - getParameter(PARAM_INTENSITY))) != 0) {
      nbEvents = 0;
    }
  }

  for (uint8_t i = 0; i < nbEvents; i++) {
    int led = random(0, MAX_LED);
    state[led] = 1;
    setColor(pixels, led);
  }
}