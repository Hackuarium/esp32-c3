#include "../tinyexpr/tinyexpr.h"
#include "./pixels.h"
#include "FastLED.h"
#include "params.h"

uint32_t epoch = 0;

char expression[256];

void updateFunction(CRGB pixels[]) {
  double x, y, i, t;
  int pixel;
  t = (double)epoch / 25;
  te_variable vars[] = {{"x", &x}, {"y", &y}, {"i", &i}, {"t", &t}};
  int err;
  te_expr* n = te_compile(expression, vars, 4, &err);

  if (n) {
    for (x = 0; x < getParameter(PARAM_NB_ROWS); x++) {
      for (y = 0; y < getParameter(PARAM_NB_COLUMNS); y++) {
        i = x * getParameter(PARAM_NB_COLUMNS) + y;
        pixel = getLedIndex(x, y);
        double result = te_eval(n);
        if (getParameter(PARAM_FCT_COLOR_MODEL) == 0) {
          result *= 255;
          if (i < MAX_LED) {
            if (result < 0) {
              pixels[pixel] = CRGB(result < -255 ? 255 : -result, 0, 0);
            } else {
              pixels[pixel] = CRGB(0, 0, result > 255 ? 255 : result);
            }
          }
        } else {
          pixels[pixel] = getHSV360((((uint16_t)result) % 360), 255, 255);
        }
      }
    }
    te_free(n);
  } else {
    for (int i = 0; i < MAX_LED; i++) {
      if ((t - round(t)) < 0) {
        pixels[i] = CRGB(255, 0, 0);
      } else {
        pixels[i] = CRGB(0, 0, 0);
      }
    }

    /* Show the user where the error is at. */
    // printf("\t%*s^\nError near here", err - 1, "");
  }
  epoch++;
}

void setFunction(char const* string) {
  // for (int i = 0; i < 256; ++i) {
  //  expression[i] = string[i];
  //}
}
