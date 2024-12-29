
#include <Adafruit_NeoPixel.h>
#include "../font53.h"
#include "../meteo.h"
#include "../pixels.h"
#include "config.h"
#include "forecast.h"
#include "taskNTPD.h"
void moonDisplay(Adafruit_NeoPixel& pixels) {
  const int size = 16;  // Size of the matrix
  const int radius = size / 2;
  static int counter = 0;  // 0 to 31
  counter++;

  uint16_t phase = (uint16_t)round(counter / 4) % 31;

  int16_t shadowXShift = phase - 15;
  // Generate a circular moon
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      int dx = x - radius;
      int dy = y - radius;
      if (dx * dx + dy * dy <= radius * radius) {
        uint16_t led = getLedIndex(y, x);
        // we calculate if the pixel is in the shadow or not
        int xRadius = abs((size - phase) / 2);
        int dxShadow = x - xRadius;
        int dyShadow = y - radius;
        if (dxShadow * dxShadow / (xRadius * xRadius) +
                dyShadow * dyShadow / (radius * radius) <=
            1) {
          pixels.setPixelColor(led, pixels.Color(31, 31, 31));
        } else {
          pixels.setPixelColor(led, pixels.Color(255, 255, 255));
        }
      }
    }
  }
}

void sunriseDisplay(Adafruit_NeoPixel& pixels) {
  pixels.clear();

  moonDisplay(pixels);
  return;

  Forecast* forecast = getForecast();
  uint32_t color = Adafruit_NeoPixel::Color(255, 127, 0);
  {
    char* sunrise = forecast->sunrise;
    // hours
    for (uint8_t i = 0; i < 2; ++i) {
      uint8_t ascii = (uint8_t)sunrise[i];
      paintSymbol(pixels, ascii, i * 4, 4, color);
    }
    // minutes
    for (uint8_t i = 3; i < 5; ++i) {
      uint8_t ascii = (uint8_t)sunrise[i];
      paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 4, color);
    }
  }
  {
    char* sunset = forecast->sunset;
    // hours
    for (uint8_t i = 0; i < 2; ++i) {
      uint8_t ascii = (uint8_t)sunset[i];
      paintSymbol(pixels, ascii, i * 4, 11, color);
    }
    // minutes
    for (uint8_t i = 3; i < 5; ++i) {
      uint8_t ascii = (uint8_t)sunset[i];
      paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 11, color);
    }
  }
}
