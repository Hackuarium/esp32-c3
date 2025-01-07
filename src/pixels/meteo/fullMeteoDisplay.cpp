#include <Adafruit_NeoPixel.h>
#include "../font53.h"
#include "../meteo.h"
#include "../pixels.h"
#include "config.h"
#include "forecast.h"
#include "taskNTPD.h"

void fullMeteoDisplay(Adafruit_NeoPixel& pixels) {
  static char* meteoTempChars = new char[6];
  pixels.clear();
  // what about the current forecast
  Forecast* forecast = getForecast();
  getHourMinute(meteoTempChars);
  uint8_t currentSlot = (uint8_t)(getHour() / 3);
  int intTemperature = forecast->current.temperature;
  String temperature =
      (intTemperature >= 0 ? "+" : "") + String(intTemperature);

  // hours
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t ascii = (uint8_t)meteoTempChars[i];
    paintSymbol(pixels, ascii, i * 4, 0);
  }
  // minutes
  for (uint8_t i = 3; i < 5; ++i) {
    uint8_t ascii = (uint8_t)meteoTempChars[i];
    paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 0);
  }

  for (uint8_t i = 0; i < temperature.length(); ++i) {
    uint8_t ascii = (uint8_t)temperature.charAt(i);
    paintSymbol(pixels, ascii, i * 4, 6);
  }

  float_t maxTemperature = forecast->temperature[0];
  for (int i = 0; i < 8; i++) {
    if (forecast->temperature[i] > maxTemperature)
      maxTemperature = forecast->temperature[i];
  }
  float_t minTemperature = forecast->temperature[0];
  for (int i = 0; i < 8; i++) {
    if (forecast->temperature[i] < minTemperature)
      minTemperature = forecast->temperature[i];
  }

  /*
    Serial.print(minTemperature);
    Serial.print(" ");
    Serial.print(maxTemperature);
    Serial.print(" ");
    Serial.println(temperature);
    */

  for (int i = 0; i < 8; i++) {
    float_t temperature = forecast->temperature[i];
    float_t rain = forecast->precipitation[i];
    float_t opacity = i == currentSlot ? 1 : 0.2;
    uint16_t weather = forecast->weather[i];

    if (minTemperature < 0) {
      int temperatureIntensity = abs((int8_t)(temperature / 2));
      for (uint8_t j = 0; j <= temperatureIntensity; j++) {
        if (temperature >= 0) {
          uint16_t led = getLedIndex(11 - j, i + 8);
          pixels.setPixelColor(led,
                               Adafruit_NeoPixel::Color(255 * opacity, 0, 0));
        } else {
          uint16_t led = getLedIndex(12 + j, i + 8);
          pixels.setPixelColor(led,
                               Adafruit_NeoPixel::Color(0, 0, 255 * opacity));
        }
      }
    } else {  // 10 degree only positive
      uint8_t tens = (uint8_t)(minTemperature / 10);
      uint8_t twos = ((uint8_t)temperature - tens * 10) / 2;
      for (uint8_t j = 0; j < tens; j++) {
        uint16_t led = getLedIndex(15 - j, i + 8);
        pixels.setPixelColor(
            led, Adafruit_NeoPixel::Color(255 * opacity, 0, 255 * opacity));
      }
      for (uint8_t j = 0; j < twos; j++) {
        uint16_t led = getLedIndex(15 - j - tens, i + 8);
        if (j < 5) {
          pixels.setPixelColor(led,
                               Adafruit_NeoPixel::Color(255 * opacity, 0, 0));
        } else {
          pixels.setPixelColor(
              led, Adafruit_NeoPixel::Color(255 * opacity, 63 * opacity,
                                            63 * opacity));
        }
      }
    }

    bool firstHalf = (millis() % 1000) < 500;
    bool firstTenth = (millis() % 1000) < 100;

    // RAIN
    int rainIntensity = 0;
    if (rain > 0.1)
      rainIntensity = 1;
    if (rain > 1)
      rainIntensity = 2;
    if (rain > 5)
      rainIntensity = 3;
    if (rain > 20)
      rainIntensity = 4;
    if (rain > 50)
      rainIntensity = 5;

    for (uint8_t j = 0; j < rainIntensity; j++) {
      uint16_t led = getLedIndex(14 - j, i);
      if (weather & RAIN_MASK) {
        pixels.setPixelColor(
            led, Adafruit_NeoPixel::Color(0, 192 * opacity, 255 * opacity));
      }
      if (weather & SNOW_MASK && (!firstHalf || (!(weather & RAIN_MASK)))) {
        pixels.setPixelColor(
            led, Adafruit_NeoPixel::Color(255 * opacity, 255 * opacity,
                                          255 * opacity));
      }
      if ((weather & LIGHTNING_MASK) && firstTenth) {
        pixels.setPixelColor(
            led, Adafruit_NeoPixel::Color(255 * opacity, 255 * opacity,
                                          0));  // yellow
      }
    }

    // GENERAL INFO

    if (weather & SUN_MASK) {
      uint16_t led = getLedIndex(15, i);
      pixels.setPixelColor(
          led, Adafruit_NeoPixel::Color(255 * opacity, 140 * opacity, 0));
    }
    if (((weather & (FOG_MASK | CLOUD_MASK)) && firstHalf) ||
        (weather & SUN_MASK) == 0) {
      uint16_t led = getLedIndex(15, i);
      pixels.setPixelColor(
          led, Adafruit_NeoPixel::Color(127 * opacity, 127 * opacity,
                                        127 * opacity));
    }
    if ((weather & LIGHTNING_MASK) && firstTenth) {
      uint16_t led = getLedIndex(15, i);
      pixels.setPixelColor(
          led,
          Adafruit_NeoPixel::Color(255 * opacity, 255 * opacity, 0));  // yellow
    }
  }
}