#include "FastLED.h"
#include "common.h"
#include "font53.h"
#include "params.h"
#include "pixels.h"
#include "taskForecast.h"
#include "taskNTPD.h"

#define SUN_MASK 0b0000000001000000
#define FOG_MASK 0b0000000000001000
#define CLOUD_MASK 0b0000000000111111
#define RAIN_MASK 0b0000000000000001
#define SNOW_MASK 0b0000000000000010
#define LIGHTNING_MASK 0b0000000000100000

void sunriseDisplay(CRGB pixels[]);
void currentDisplay(CRGB pixels[]);
void fullMeteoDisplay(CRGB pixels[]);
void compact(CRGB pixels[]);

char* hourMinute = new char[6];

void updateMeteo(CRGB pixels[]) {
  // Display the time

  blank(pixels);

  if (getParameter(PARAM_NB_COLUMNS) < 16) {
    compact(pixels);
    return;
  }

  uint8_t slot = floor((getSeconds() % 30) / 5);
  uint8_t intensity = (getSeconds() % 30) - slot * 5;
  switch (slot) {
    case 0:  // sunrise, sunset
      sunriseDisplay(pixels);
      break;
    case 999:  // disabled current temperature / humidity
      currentDisplay(pixels);
      break;
    default:
      fullMeteoDisplay(pixels);
  }
}

void currentDisplay(CRGB pixels[]) {
  getDayMonth(hourMinute);
  uint8_t currentSlot = (uint8_t)(getHour() / 3);
  // day
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t ascii = (uint8_t)hourMinute[i];
    paintSymbol(pixels, ascii, i * 4, 0, CRGB(0xe2, 0xd8, 0x10));
  }
  // month
  for (uint8_t i = 3; i < 5; ++i) {
    uint8_t ascii = (uint8_t)hourMinute[i];
    paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 0, CRGB(0xe2, 0xd8, 0x10));
  }

  float_t* currentWeather = getCurrentWeather();

  if (isnan(currentWeather[0])) {
    fullMeteoDisplay(pixels);
    return;
  }
  int intTemperature = round(currentWeather[0]);
  String temperature =
      (intTemperature >= 0 ? "+" : "") + String(intTemperature);

  for (uint8_t i = 0; i < temperature.length(); ++i) {
    uint8_t ascii = (uint8_t)temperature.charAt(i);
    paintSymbol(pixels, ascii, i * 4, 6, CRGB(0xd9, 0x13, 0x8a));
  }

  int intHumidity = round(currentWeather[1]);
  String humidity = String(intHumidity) + "%";
  for (uint8_t i = 0; i < humidity.length(); ++i) {
    uint8_t ascii = (uint8_t)humidity.charAt(i);
    paintSymbol(pixels, ascii, 5 + i * 4, 11, CRGB(0x12, 0xa4, 0xd9));
  }
}

void sunriseDisplay(CRGB pixels[]) {
  CRGB color = CRGB(255, 127, 0);
  {
    char* sunrise = getSunrise();
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
    char* sunset = getSunset();
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

void fullMeteoDisplay(CRGB pixels[]) {
  // what about the current forecast
  float_t* forecast = getForecast();
  getHourMinute(hourMinute);
  uint8_t currentSlot = (uint8_t)(getHour() / 3);
  int intTemperature = round(forecast[0]);
  String temperature =
      (intTemperature >= 0 ? "+" : "") + String(intTemperature);

  // hours
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t ascii = (uint8_t)hourMinute[i];
    paintSymbol(pixels, ascii, i * 4, 0);
  }
  // minutes
  for (uint8_t i = 3; i < 5; ++i) {
    uint8_t ascii = (uint8_t)hourMinute[i];
    paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 0);
  }

  for (uint8_t i = 0; i < temperature.length(); ++i) {
    uint8_t ascii = (uint8_t)temperature.charAt(i);
    paintSymbol(pixels, ascii, i * 4, 6);
  }

  float_t maxTemperature = forecast[4];
  for (int i = 8; i < 36; i = i + 4) {
    if (forecast[i] > maxTemperature)
      maxTemperature = forecast[i];
  }
  float_t minTemperature = forecast[4];
  for (int i = 8; i < 36; i = i + 4) {
    if (forecast[i] < minTemperature)
      minTemperature = forecast[i];
  }

  for (int i = 4; i < 36; i = i + 4) {
    float_t temperature = forecast[i];
    float_t rain = forecast[i + 1];
    uint16_t weather = forecast[i + 2];
    // int8_t wind = forecast[i + 3];
    float_t dim = (i - 4) / 4 == currentSlot ? 1 : 0.2;
    // TEMPERATURE

    if (minTemperature < 0) {  // currentTemperature under 10
      int temperatureIntensity = abs((int8_t)(temperature / 2));
      for (uint8_t j = 0; j <= temperatureIntensity; j++) {
        if (temperature >= 0) {
          uint16_t led = getLedIndex(11 - j, i / 4 + 7);
          pixels[led] = CRGB(255 * dim, 0, 0);
        } else {
          uint16_t led = getLedIndex(12 + j, i / 4 + 7);
          pixels[led] = CRGB(0, 0, 255 * dim);
        }
      }
    } else {  // 10 degree only positive
      uint8_t tens = (uint8_t)(minTemperature / 10);
      uint8_t twos = ((uint8_t)temperature - tens * 10) / 2;
      for (uint8_t j = 0; j < tens; j++) {
        uint16_t led = getLedIndex(15 - j, i / 4 + 7);
        pixels[led] = CRGB(255 * dim, 0, 255 * dim);
      }
      for (uint8_t j = 0; j < twos; j++) {
        uint16_t led = getLedIndex(15 - j - tens, i / 4 + 7);
        if (j < 5) {
          pixels[led] = CRGB(255 * dim, 0, 0);
        } else {
          pixels[led] = CRGB(255 * dim, 63 * dim, 63 * dim);
        }
      }
    }

    /*
     int temperatureIntensity = abs((uint8_t)(temperature / 1));
        for (uint8_t j = 0; j <= temperatureIntensity; j++) {
          uint16_t led = getLedIndex(7 - j+4, i / 4 +7);
          if (temperature >= 0) {
               pixels[led]=CRGB(255,0,0);
          } else {
               pixels[led]=CRGB(0,0,255);
          }
        }
    */

    bool firstHalf = (millis() % 1000) < 500;
    bool firstTenth = (millis() % 1000) < 100;

    // RAIN
    int rainIntensity = rain < 0.1 ? 0 : (log10(rain) / log10(3) + 3);
    for (uint8_t j = 0; j < rainIntensity; j++) {
      if (weather & RAIN_MASK) {
        uint16_t led = getLedIndex(14 - j, i / 4 - 1);
        pixels[led] = CRGB(0, 192 * dim, 255 * dim);
      }
      if (weather & SNOW_MASK && (!firstHalf || (!(weather & RAIN_MASK)))) {
        uint16_t led = getLedIndex(14 - j, i / 4 - 1);
        pixels[led] = CRGB(255 * dim, 255 * dim, 255 * dim);
      }
      if ((weather & LIGHTNING_MASK) && firstTenth) {
        uint16_t led = getLedIndex(14 - j, i / 4 - 1);
        pixels[led] = CRGB(255 * dim, 255 * dim, 0);
      }
    }

    // GENERAL INFO

    if (weather & SUN_MASK) {
      uint16_t led = getLedIndex(15, i / 4 - 1);
      pixels[led] = CRGB(255 * dim, 140 * dim, 0);
    }
    if (((weather & (FOG_MASK | CLOUD_MASK)) && firstHalf) ||
        (weather & SUN_MASK) == 0) {
      uint16_t led = getLedIndex(15, i / 4 - 1);
      pixels[led] = CRGB(127 * dim, 127 * dim, 127 * dim);
    }
  }
}

void compact(CRGB pixels[]) {
  uint8_t slot = floor((getSeconds() % 20) / 5);

  if (false) {
    // Display the seconds
    uint8_t seconds = getSeconds() * 17 / 60;
    for (uint8_t i = 0; i < 16; i++) {
      uint16_t led = getLedIndex(6, i);
      if (i < seconds) {
        pixels[led] = CRGB(255, 255, 255);
      } else {
        pixels[led] = CRGB(0, 0, 0);
      }
    }
  }

  // what about the current forecast
  float_t* forecast = getForecast();
  getHourMinute(hourMinute);
  String temperature =
      (forecast[0] >= 0 ? "+" : "") + String((int)round(forecast[0]));

  switch (slot) {
    case 0:
      // hours
      for (uint8_t i = 0; i < 2; ++i) {
        uint8_t ascii = (uint8_t)hourMinute[i];
        paintSymbol(pixels, ascii, i * 4, 0);
      }
      break;
    case 1:
      // minutes
      for (uint8_t i = 0; i < 2; ++i) {
        uint8_t ascii = (uint8_t)hourMinute[i + 3];
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

  for (int i = 4; i < 36; i = i + 4) {
    float_t temperature = forecast[i];
    float_t rain = forecast[i + 1];
    uint16_t weather = forecast[i + 2];
    // int8_t wind = forecast[i + 3];

    if (slot == 0) {  // TEMPERATURE
      int temperatureIntensity = abs((uint8_t)(temperature / 5));
      for (uint8_t j = 0; j <= temperatureIntensity; j++) {
        uint16_t led = getLedIndex(7 - j, i / 4 - 1);
        if (temperature >= 0) {
          pixels[led] = CRGB(255, 0, 0);
        } else {
          pixels[led] = CRGB(0, 0, 255);
        }
      }
    } else if (slot == 1) {  // RAIN
      int rainIntensity = rain < 0.1 ? 1 : floor(log10(rain) + 3);
      for (uint8_t j = 0; j < rainIntensity; j++) {
        uint16_t led = getLedIndex(7 - j, i / 4 - 1);
        pixels[led] = CRGB(0, 255, 0);
      }
    } else {  // GENERAL INFO
      bool firstHalf = (millis() % 1000) < 500;
      bool firstTenth = (millis() % 1000) < 100;
      if (weather & SUN_MASK) {
        uint16_t led = getLedIndex(7, i / 4 - 1);
        pixels[led] = CRGB(255, 140, 0);
      }
      if (((weather & (FOG_MASK | CLOUD_MASK)) && firstHalf) ||
          (weather & SUN_MASK) == 0) {
        uint16_t led = getLedIndex(7, i / 4 - 1);
        pixels[led] = CRGB(127, 127, 127);
      }
      if (weather & RAIN_MASK && firstHalf) {
        uint16_t led = getLedIndex(6, i / 4 - 1);
        pixels[led] = CRGB(0, 0, 255);
      }
      if (weather & SNOW_MASK && !firstHalf) {
        uint16_t led = getLedIndex(6, i / 4 - 1);
        pixels[led] = CRGB(255, 255, 255);
      }
      if ((weather & LIGHTNING_MASK) && firstTenth) {
        uint16_t led = getLedIndex(6, i / 4 - 1);
        pixels[led] = CRGB(255, 255, 0);
      }
    }
  }
}