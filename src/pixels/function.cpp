#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "../tinyexpr/tinyexpr.h"
#include "./pixels.h"
#include "params.h"

uint32_t epoch = 0;

char expression[256];

void setFunction(const char* string) {
  for (int i = 0; i < 256; ++i) {
    expression[i] = string[i];
  }
}

void updateFunction(Adafruit_NeoPixel& pixels) {
  double x, y, i, t;
  int led;
  t = (double)epoch / 25;
  te_variable vars[] = {{"x", &x}, {"y", &y}, {"i", &i}, {"t", &t}};
  int err;

  // TODO fix this code
  return;

  te_expr* n = te_compile(expression, vars, 4, &err);

  if (n) {
    for (x = 0; x < getParameter(PARAM_NB_ROWS); x++) {
      for (y = 0; y < getParameter(PARAM_NB_COLUMNS); y++) {
        i = x * getParameter(PARAM_NB_COLUMNS) + y;
        led = getLedIndex(x, y);
        double result = te_eval(n);
        if (getParameter(PARAM_FCT_COLOR_MODEL) == 0) {
          result *= 255;
          if (i < MAX_LED) {
            if (result < 0) {
              pixels.setPixelColor(led, result < -255 ? 255 : -result, 0, 0);
            } else {
              pixels.setPixelColor(led, 0, 0, result > 255 ? 255 : result);
            }
          }
        } else {
          pixels.setPixelColor(led,
                               getHSV360((((uint16_t)result) % 360), 255, 255));
        }
      }
    }
    te_free(n);
  } else {
    for (int i = 0; i < MAX_LED; i++) {
      if ((t - round(t)) < 0) {
        pixels.setPixelColor(i, 255, 0, 0);
      } else {
        pixels.setPixelColor(i, 0, 0, 0);
      }
    }
  }
  epoch++;
}

#endif