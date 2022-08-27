#include <FastLED.h>
#include "./common.h"
#include "./params.h"
#include "./pixels.h"

void updateRGB(CRGB pixels[]) {
  for (int led = 0; led < MAX_LED; led++) {
    pixels[led] =
        CRGB(getParameter(PARAM_LED_RED), getParameter(PARAM_LED_GREEN),
             getParameter(PARAM_LED_BLUE));
  }
}