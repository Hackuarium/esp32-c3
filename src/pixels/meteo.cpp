#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "font53.h"
#include "forecast.h"

#include "gif.h"
#include "meteo.h"
#include "params.h"
#include "pixels.h"
#include "taskNTPD.h"

const uint8_t SLOT_METEO[6] = {PARAM_METEO_SLOT_1, PARAM_METEO_SLOT_2,
                               PARAM_METEO_SLOT_3, PARAM_METEO_SLOT_4,
                               PARAM_METEO_SLOT_5, PARAM_METEO_SLOT_6};

void sunriseDisplay(Adafruit_NeoPixel& pixels);
void currentDisplay(Adafruit_NeoPixel& pixels);
void iconDisplay(Adafruit_NeoPixel& pixels);
void fullMeteoDisplay(Adafruit_NeoPixel& pixels);
void windDisplay(Adafruit_NeoPixel& pixels);
void froniusDisplay(Adafruit_NeoPixel& pixels, uint16_t counter);
void compactMeteoDisplay(Adafruit_NeoPixel& pixels);

char* meteoTempChars = new char[6];
char* windDirectionChars = new char[3];

void updateMeteo(Adafruit_NeoPixel& pixels, uint16_t counter) {
  // Display the time

  if (getParameter(PARAM_NB_COLUMNS) < 16) {
    compactMeteoDisplay(pixels);
    return;
  }

  uint8_t slot = floor((getSeconds() % 30) / 5);
  uint16_t task = getParameter(SLOT_METEO[slot]);
  uint8_t intensity = (getSeconds() % 30) - slot * 5;
  // there are 6 slots
  switch (task) {
    case SLOT_METEO_WEATHER_ICON:  // weather icon
      iconDisplay(pixels);
      break;
    case SLOT_METEO_FRONIUS:
      froniusDisplay(pixels, counter);
      break;
    case SLOT_METEO_SUNRISE_SUNSET:
      sunriseDisplay(pixels);
      break;
    case SLOT_METEO_WIND:
      windDisplay(pixels);
      break;
    default:
      // froniusDisplay(pixels, counter);
      fullMeteoDisplay(pixels);
  }
}

void currentDisplay(Adafruit_NeoPixel& pixels) {
  getDayMonth(meteoTempChars);
  uint8_t currentSlot = (uint8_t)(getHour() / 3);
  // day
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t ascii = (uint8_t)meteoTempChars[i];
    paintSymbol(pixels, ascii, i * 4, 0,
                Adafruit_NeoPixel::Color(0xe2, 0xd8, 0x10));
  }
  // month
  for (uint8_t i = 3; i < 5; ++i) {
    uint8_t ascii = (uint8_t)meteoTempChars[i];
    paintSymbol(pixels, ascii, (i - 1) * 4 + 1, 0,
                Adafruit_NeoPixel::Color(0xe2, 0xd8, 0x10));
  }
  // we retrieve the forecast and an instance of Forecast
  Forecast* forecast = getForecast();

  if (isnan(forecast->current.temperature)) {
    fullMeteoDisplay(pixels);
    return;
  }

  int intTemperature = forecast->current.temperature;
  String temperature =
      (intTemperature >= 0 ? "+" : "") + String(intTemperature);

  for (uint8_t i = 0; i < temperature.length(); ++i) {
    uint8_t ascii = (uint8_t)temperature.charAt(i);
    paintSymbol(pixels, ascii, i * 4, 6,
                Adafruit_NeoPixel::Color(0xd9, 0x13, 0x8a));
  }

  /*
    int intHumidity = round(currentWeather[1]);
    String humidity = String(intHumidity) + "%";
    for (uint8_t i = 0; i < humidity.length(); ++i) {
      uint8_t ascii = (uint8_t)humidity.charAt(i);
      paintSymbol(pixels, ascii, 5 + i * 4, 11,
                  Adafruit_NeoPixel::Color(0x12, 0xa4, 0xd9));
    }
    */
}

void sunriseDisplay(Adafruit_NeoPixel& pixels) {
  pixels.clear();
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

char gifPath[50];

void iconDisplay(Adafruit_NeoPixel& pixels) {
  Forecast* forecast = getForecast();
  sprintf(gifPath, "/gifs/weather/%d.gif", forecast->nextIcon);
  setGIF(gifPath);

  updateGif(pixels);
}

#endif