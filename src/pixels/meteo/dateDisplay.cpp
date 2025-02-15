
#include <Adafruit_NeoPixel.h>
#include "../font53.h"
#include "../meteo.h"
#include "../pixels.h"
#include "config.h"
#include "forecast.h"
#include "moonDisplay.h"
#include "taskNTPD.h"

void dateDisplay(Adafruit_NeoPixel& pixels) {
  pixels.clear();
  moonDisplay(pixels);

  Forecast* forecast = getForecast();
  uint32_t color = Adafruit_NeoPixel::Color(0, 255, 0);
  {
    char* dayMonth = forecast->dayMonth;
    // hours
    for (uint8_t i = 0; i < 2; ++i) {
      uint8_t ascii = (uint8_t)dayMonth[i];
      paintSymbol(pixels, ascii, i * 4, 5, color);
    }
    // minutes
    for (uint8_t i = 3; i < 5; ++i) {
      uint8_t ascii = (uint8_t)dayMonth[i];
      paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 5, color);
    }
  }
}
