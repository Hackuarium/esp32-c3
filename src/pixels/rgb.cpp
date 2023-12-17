#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "params.h"

void updateRGB(Adafruit_NeoPixel& pixels) {
  for (int led = 0; led < MAX_LED; led++) {
    pixels.setPixelColor(led, getParameter(PARAM_LED_RED),
                         getParameter(PARAM_LED_GREEN),
                         getParameter(PARAM_LED_BLUE));
  }
}

#endif