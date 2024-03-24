#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "font53.h"
#include "gif.h"
#include "params.h"
#include "pixels.h"
#include "taskForecast.h"
#include "taskFronius.h"
#include "taskNTPD.h"

#define SUN_MASK 0b0000000001000000
#define FOG_MASK 0b0000000000001000
#define CLOUD_MASK 0b0000000000111111
#define RAIN_MASK 0b0000000000000001
#define SNOW_MASK 0b0000000000000010
#define LIGHTNING_MASK 0b0000000000100000

void sunriseDisplay(Adafruit_NeoPixel& pixels);
void currentDisplay(Adafruit_NeoPixel& pixels);
void iconDisplay(Adafruit_NeoPixel& pixels);
void fullMeteoDisplay(Adafruit_NeoPixel& pixels);
void froniusDisplay(Adafruit_NeoPixel& pixels);
void compact(Adafruit_NeoPixel& pixels);

char* hourMinute = new char[6];

void updateMeteo(Adafruit_NeoPixel& pixels) {
  // Display the time

  if (getParameter(PARAM_NB_COLUMNS) < 16) {
    compact(pixels);
    return;
  }

  uint8_t slot = floor((getSeconds() % 30) / 5);
  uint8_t intensity = (getSeconds() % 30) - slot * 5;
  // there are 6 slots
  switch (slot) {
    case 0:  // weather icon
      iconDisplay(pixels);
      break;
    case 3:
      froniusDisplay(pixels);
      break;
    case 200:  // sunrise, sunset
      sunriseDisplay(pixels);
      break;
    case 201:  // disabled current temperature / humidity
      currentDisplay(pixels);
      break;
    default:
      pixels.clear();
      // froniusDisplay(pixels);
      fullMeteoDisplay(pixels);
  }
}

uint8_t rowLevels[16] = {0, 0, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 4, 3, 2, 1};
uint8_t columnLevels[16] = {0, 1, 2, 3, 4, 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0};

void paintSquare(Adafruit_NeoPixel& pixels,
                 uint8_t row,
                 uint8_t column,
                 uint32_t color,
                 uint32_t backgroundColor,
                 uint8_t level) {
  for (uint8_t i = 0; i < 5; ++i) {
    for (uint8_t j = 0; j < 5; ++j) {
      pixels.setPixelColor(getLedIndex(row + i, column + j), backgroundColor);
    }
  }
  pixels.setPixelColor(getLedIndex(row + 2, column + 2), color);
  if (level > 16)
    level = 16;
  for (uint8_t i = 0; i < level; ++i) {
    pixels.setPixelColor(
        getLedIndex(row + rowLevels[i], column + columnLevels[i]), color);
    pixels.setPixelColor(
        getLedIndex(row + rowLevels[i], column + columnLevels[i]), color);
  }
}

void froniusDisplay(Adafruit_NeoPixel& pixels) {
  pixels.clear();
  FroniusStatus status = getFroniusStatus();
  // PV
  paintSquare(pixels, 0, 0, Adafruit_NeoPixel::Color(0xff, 0xff, 0x00),
              Adafruit_NeoPixel::Color(0x33, 0x33, 0x00),
              (uint8_t)round(status.powerFromPV / 500));
  // Battery
  paintSquare(pixels, 0, 11, Adafruit_NeoPixel::Color(0x00, 0xff, 0x00),
              Adafruit_NeoPixel::Color(0x00, 0x33, 0x00),
              (uint8_t)round(status.batteryChargePercentage / 6));
  // Network
  int16_t networkLevel = (uint8_t)round(status.powerFromGrid / 500);
  if (networkLevel < 0)
    networkLevel = -networkLevel;
  paintSquare(pixels, 11, 0, Adafruit_NeoPixel::Color(0xff, 0xff, 0xff),
              Adafruit_NeoPixel::Color(0x33, 0x33, 0x33), networkLevel);
  // Consumption
  paintSquare(pixels, 11, 11, Adafruit_NeoPixel::Color(0xff, 0x00, 0x00),
              Adafruit_NeoPixel::Color(0x33, 0x00, 0x00),
              (uint8_t)round(status.currentLoad / 500));
}

void currentDisplay(Adafruit_NeoPixel& pixels) {
  getDayMonth(hourMinute);
  uint8_t currentSlot = (uint8_t)(getHour() / 3);
  // day
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t ascii = (uint8_t)hourMinute[i];
    paintSymbol(pixels, ascii, i * 4, 0,
                Adafruit_NeoPixel::Color(0xe2, 0xd8, 0x10));
  }
  // month
  for (uint8_t i = 3; i < 5; ++i) {
    uint8_t ascii = (uint8_t)hourMinute[i];
    paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 0,
                Adafruit_NeoPixel::Color(0xe2, 0xd8, 0x10));
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
    paintSymbol(pixels, ascii, i * 4, 6,
                Adafruit_NeoPixel::Color(0xd9, 0x13, 0x8a));
  }

  int intHumidity = round(currentWeather[1]);
  String humidity = String(intHumidity) + "%";
  for (uint8_t i = 0; i < humidity.length(); ++i) {
    uint8_t ascii = (uint8_t)humidity.charAt(i);
    paintSymbol(pixels, ascii, 5 + i * 4, 11,
                Adafruit_NeoPixel::Color(0x12, 0xa4, 0xd9));
  }
}

void sunriseDisplay(Adafruit_NeoPixel& pixels) {
  uint32_t color = Adafruit_NeoPixel::Color(255, 127, 0);
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

char gifPath[50];

void iconDisplay(Adafruit_NeoPixel& pixels) {
  float_t* forecast = getForecast();
  sprintf(gifPath, "/gifs/weather/%d.gif", (int)forecast[2]);
  setGIF(gifPath);

  updateGif(pixels);
}

void fullMeteoDisplay(Adafruit_NeoPixel& pixels) {
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
          pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255 * dim, 0, 0));
        } else {
          uint16_t led = getLedIndex(12 + j, i / 4 + 7);
          pixels.setPixelColor(led, Adafruit_NeoPixel::Color(0, 0, 255 * dim));
        }
      }
    } else {  // 10 degree only positive
      uint8_t tens = (uint8_t)(minTemperature / 10);
      uint8_t twos = ((uint8_t)temperature - tens * 10) / 2;
      for (uint8_t j = 0; j < tens; j++) {
        uint16_t led = getLedIndex(15 - j, i / 4 + 7);
        pixels.setPixelColor(led,
                             Adafruit_NeoPixel::Color(255 * dim, 0, 255 * dim));
      }
      for (uint8_t j = 0; j < twos; j++) {
        uint16_t led = getLedIndex(15 - j - tens, i / 4 + 7);
        if (j < 5) {
          pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255 * dim, 0, 0));
        } else {
          pixels.setPixelColor(
              led, Adafruit_NeoPixel::Color(255 * dim, 63 * dim, 63 * dim));
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
      if (weather & RAIN_MASK) {
        uint16_t led = getLedIndex(14 - j, i / 4 - 1);
        pixels.setPixelColor(led,
                             Adafruit_NeoPixel::Color(0, 192 * dim, 255 * dim));
      }
      if (weather & SNOW_MASK && (!firstHalf || (!(weather & RAIN_MASK)))) {
        uint16_t led = getLedIndex(14 - j, i / 4 - 1);
        pixels.setPixelColor(
            led, Adafruit_NeoPixel::Color(255 * dim, 255 * dim, 255 * dim));
      }
      if ((weather & LIGHTNING_MASK) && firstTenth) {
        uint16_t led = getLedIndex(14 - j, i / 4 - 1);
        pixels.setPixelColor(led,
                             Adafruit_NeoPixel::Color(255 * dim, 255 * dim, 0));
      }
    }

    // GENERAL INFO

    if (weather & SUN_MASK) {
      uint16_t led = getLedIndex(15, i / 4 - 1);
      pixels.setPixelColor(led,
                           Adafruit_NeoPixel::Color(255 * dim, 140 * dim, 0));
    }
    if (((weather & (FOG_MASK | CLOUD_MASK)) && firstHalf) ||
        (weather & SUN_MASK) == 0) {
      uint16_t led = getLedIndex(15, i / 4 - 1);
      pixels.setPixelColor(
          led, Adafruit_NeoPixel::Color(127 * dim, 127 * dim, 127 * dim));
    }
  }
}

void compact(Adafruit_NeoPixel& pixels) {
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

#endif