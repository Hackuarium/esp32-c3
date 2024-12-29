#include <Adafruit_NeoPixel.h>
#include "../font53.h"
#include "../meteo.h"
#include "../pixels.h"
#include "config.h"
#include "forecast.h"
#include "taskNTPD.h"

void compact(Adafruit_NeoPixel& pixels) {
  static char* meteoTempChars = new char[6];
  uint8_t slot = floor((getSeconds() % 20) / 5);

  if (false) {
    // Display the seconds
    uint8_t seconds = getSeconds() * 17 / 60;
    for (uint8_t i = 0; i < 16; i++) {
      uint16_t led = getLedIndex(6, i);
      if (i < seconds) {
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255, 255, 255));
      } else {
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(0, 0, 0));
      }
    }
  }

  Forecast* forecast = getForecast();

  getHourMinute(meteoTempChars);
  String temperature = (forecast->current.temperature >= 0 ? "+" : "") +
                       String(forecast->current.temperature);

  switch (slot) {
    case 0:
      // hours
      for (uint8_t i = 0; i < 2; ++i) {
        uint8_t ascii = (uint8_t)meteoTempChars[i];
        paintSymbol(pixels, ascii, i * 4, 0);
      }
      break;
    case 1:
      // minutes
      for (uint8_t i = 0; i < 2; ++i) {
        uint8_t ascii = (uint8_t)meteoTempChars[i + 3];
        paintSymbol(pixels, ascii, i * 4, 0);
      }
      break;
    case 2:
    case 3:
      // display current temperature
      for (uint8_t i = 0; i < temperature.length(); ++i) {
        uint8_t ascii = (uint8_t)temperature.charAt(i);
        paintSymbol(pixels, ascii, i * 4, 0);
      }
      break;
  }

  for (int i = 0; i < 8; i++) {
    float_t temperature = forecast->temperature[i];
    float_t rain = forecast->precipitation[i];
    uint16_t weather = forecast->weather[i];

    if (slot == 0) {  // TEMPERATURE
      int temperatureIntensity = abs((uint8_t)(temperature / 5));
      for (uint8_t j = 0; j <= temperatureIntensity; j++) {
        uint16_t led = getLedIndex(7 - j, i / 4 - 1);
        if (temperature >= 0) {
          pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255, 0, 0));
        } else {
          pixels.setPixelColor(led, Adafruit_NeoPixel::Color(0, 0, 255));
        }
      }
    } else if (slot == 1) {  // RAIN
      int rainIntensity = rain < 0.1 ? 1 : floor(log10(rain) + 3);
      for (uint8_t j = 0; j < rainIntensity; j++) {
        uint16_t led = getLedIndex(7 - j, i / 4 - 1);
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(0, 255, 0));
      }
    } else {  // GENERAL INFO
      bool firstHalf = (millis() % 1000) < 500;
      bool firstTenth = (millis() % 1000) < 100;
      if (weather & SUN_MASK) {
        uint16_t led = getLedIndex(7, i / 4 - 1);
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255, 140, 0));
      }
      if (((weather & (FOG_MASK | CLOUD_MASK)) && firstHalf) ||
          (weather & SUN_MASK) == 0) {
        uint16_t led = getLedIndex(7, i / 4 - 1);
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(127, 127, 127));
      }
      if (weather & RAIN_MASK && firstHalf) {
        uint16_t led = getLedIndex(6, i / 4 - 1);
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(0, 0, 255));
      }
      if (weather & SNOW_MASK && !firstHalf) {
        uint16_t led = getLedIndex(6, i / 4 - 1);
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255, 255, 255));
      }
      if ((weather & LIGHTNING_MASK) && firstTenth) {
        uint16_t led = getLedIndex(6, i / 4 - 1);
        pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255, 255, 0));
      }
    }
  }
}