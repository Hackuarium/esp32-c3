
#include <Adafruit_NeoPixel.h>
#include "../font53.h"
#include "../meteo.h"
#include "../pixels.h"
#include "config.h"
#include "forecast.h"
#include "taskNTPD.h"

void moonDisplay(Adafruit_NeoPixel& pixels) {
  const int size = 16;  // Size of the matrix
  const float radius = (size - 1) / 2;

  Forecast* forecast = getForecast();

  uint16_t phase = floor(forecast->lunarAge);

  if (phase > 14)
    phase++;

  int8_t left_inside = 0;
  int8_t left_outside = 0;
  int8_t right_inside = 0;
  int8_t right_outside = 0;

  static int8_t shadow = 16;
  static int8_t light = 64;

  float quarter = (float)(phase % 8) / 7;
  float xRadius = 0;
  if (phase <= 7) {
    left_inside = shadow;
    left_outside = shadow;
    right_inside = shadow;
    right_outside = light;
    xRadius = radius * (1 - quarter);
  } else if (phase <= 15) {
    left_inside = light;
    left_outside = shadow;
    right_inside = light;
    right_outside = light;
    xRadius = radius * quarter;
  } else if (phase <= 23) {
    left_inside = light;
    left_outside = light;
    right_inside = light;
    right_outside = shadow;
    xRadius = radius * (1 - quarter);
  } else {
    left_inside = shadow;
    left_outside = light;
    right_inside = shadow;
    right_outside = shadow;
    xRadius = radius * quarter;
  }

  // Generate a circular moon
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      float dx = x - radius;
      float dy = y - radius;
      if (dx * dx + dy * dy <= radius * radius * 1.05) {
        uint16_t led = getLedIndex(y, x);

        if (dx * dx / (xRadius * xRadius) + dy * dy / (radius * radius * 1.1) <=
            1) {             // inside the ellipse
          if (x < radius) {  // left
            pixels.setPixelColor(
                led, pixels.Color(left_inside, left_inside, left_inside));
          } else {  // right
            pixels.setPixelColor(
                led, pixels.Color(right_inside, right_inside, right_inside));
          }
        } else {             // outside the ellipse
          if (x < radius) {  // left
            pixels.setPixelColor(
                led, pixels.Color(left_outside, left_outside, left_outside));
          } else {  // right
            pixels.setPixelColor(
                led, pixels.Color(right_outside, right_outside, right_outside));
          }
        }
      }
    }
  }
}
