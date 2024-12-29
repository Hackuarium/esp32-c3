#include <Adafruit_NeoPixel.h>
#include "../font53.h"
#include "../meteo.h"
#include "../pixels.h"
#include "config.h"
#include "forecast.h"
#include "taskNTPD.h"

void windDisplay(Adafruit_NeoPixel& pixels) {
  static char* meteoTempChars = new char[6];
  pixels.clear();
  Forecast* forecast = getForecast();
  uint32_t color = Adafruit_NeoPixel::Color(0, 255, 255);
  uint8_t currentSlot = (uint8_t)(getHour() / 3);
  // display current wind speed as text
  int16_t windSpeed = forecast->current.windSpeed;
  itoa(windSpeed, meteoTempChars, 10);
  uint8_t length = 0;
  while (meteoTempChars[length] != '\0') {
    length++;
  }

  for (uint8_t i = 0; i < length; ++i) {
    uint8_t ascii = (uint8_t)meteoTempChars[i];
    paintSymbol(pixels, ascii, 17 - (length - i) * 4, 0);
  }

  // display the wind direction as text stored in current.windDirectionText
  length = 0;
  while (forecast->current.windDirectionText[length] != '\0') {
    length++;
  }
  for (uint8_t i = 0; i < length; ++i) {
    uint8_t ascii = (uint8_t)forecast->current.windDirectionText[i];
    paintSymbol(pixels, ascii, 17 - (length - i) * 4, 6);
  }

  // display a little bar graph for the wind speed
  for (uint8_t i = 0; i < 8; ++i) {
    float_t opacity = i == currentSlot ? 1 : 0.2;
    int16_t windSpeed = forecast->windSpeed[i];
    if (windSpeed == 0)
      windSpeed = 1;

    for (uint8_t j = 0; j < forecast->windSpeedLog2[i]; j++) {
      pixels.setPixelColor(getLedIndex(15 - j, i),
                           Adafruit_NeoPixel::Color(0, 255 * opacity, 0));
    }
  }

  // display the big arrow starting at 4,4
  pixels.setPixelColor(getLedIndex(4, 4),
                       Adafruit_NeoPixel::Color(0, 255, 255));
  uint8_t currentRow = 4;
  uint8_t currentColumn = 4;
  for (uint8_t i = 0; i < forecast->current.windSpeedLog2; i++) {
    currentRow += forecast->current.windDirectionRow;
    currentColumn += forecast->current.windDirectionColumn;
    pixels.setPixelColor(getLedIndex(currentRow, currentColumn),
                         Adafruit_NeoPixel::Color(0, 63, 63));
  }

  // finally we display some little 2x2 squares for wind direction prediction
  uint8_t slot = currentSlot;
  for (uint8_t row = 11; row <= 14; row += 3) {
    for (uint8_t column = 8; column <= 14; column += 3) {
      uint8_t headRow = row;
      uint8_t headColumn = column;
      uint8_t tailRow = row;
      uint8_t tailColumn = column;

      slot = (slot + 1) % 8;
      uint8_t windDirection07 = forecast->windDirection07[slot];
      int8_t windDirectionRow = getWindRowDirection(windDirection07);
      int8_t windDirectionColumn = getWindColumnDirection(windDirection07);

      if (windDirectionRow >= 0) {
        tailRow += windDirectionRow;
      } else {
        headRow += 1;
      }
      if (windDirectionColumn >= 0) {
        tailColumn += windDirectionColumn;
      } else {
        headColumn += 1;
      }
      pixels.setPixelColor(getLedIndex(headRow, headColumn),
                           Adafruit_NeoPixel::Color(0, 255, 255));
      pixels.setPixelColor(getLedIndex(tailRow, tailColumn),
                           Adafruit_NeoPixel::Color(0, 63, 63));
      // pixels.setPixelColor(
      //    getLedIndex(row + windDirectionRow, column + windDirectionColumn),
      //   Adafruit_NeoPixel::Color(0, 0, 255));
    }
  }
}