#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "font53.h"
#include "forecast.h"
#include "fronius.h"
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
void compact(Adafruit_NeoPixel& pixels);

char* meteoTempChars = new char[6];
char* windDirectionChars = new char[3];

void updateMeteo(Adafruit_NeoPixel& pixels, uint16_t counter) {
  // Display the time

  if (getParameter(PARAM_NB_COLUMNS) < 16) {
    compact(pixels);
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

void paintSquare(Adafruit_NeoPixel& pixels,
                 uint8_t row,
                 uint8_t column,
                 uint32_t highColor,
                 uint32_t lowColor,
                 uint32_t backgroundColor,
                 uint8_t highValue,
                 uint8_t lowValue) {
  if (highValue > 16) {  // we fill completely the square with bright color
    for (uint8_t i = 0; i < 5; ++i) {
      for (uint8_t j = 0; j < 5; ++j) {
        pixels.setPixelColor(getLedIndex(row + i, column + j), highColor);
      }
    }
    return;
  }

  static uint8_t rowLevels[16] = {0, 0, 0, 0, 0, 1, 2, 3,
                                  4, 4, 4, 4, 4, 3, 2, 1};
  static uint8_t columnLevels[16] = {0, 1, 2, 3, 4, 4, 4, 4,
                                     4, 3, 2, 1, 0, 0, 0, 0};

  for (uint8_t i = 0; i < 5; ++i) {
    for (uint8_t j = 0; j < 5; ++j) {
      pixels.setPixelColor(getLedIndex(row + i, column + j), backgroundColor);
    }
  }

  for (uint8_t i = 0; i < highValue; ++i) {
    pixels.setPixelColor(
        getLedIndex(row + rowLevels[i], column + columnLevels[i]), highColor);
    pixels.setPixelColor(
        getLedIndex(row + rowLevels[i], column + columnLevels[i]), highColor);
  }

  // details information in the centrum
  for (uint8_t i = 0; i < min(lowValue, (uint8_t)9); ++i) {
    pixels.setPixelColor(
        getLedIndex(row + 1 + floor(i / 3), column + 1 + i % 3), lowColor);
  }
}

void paintFlux(Adafruit_NeoPixel& pixels,
               uint8_t fromRow,
               uint8_t fromColumn,
               uint8_t toRow,
               uint8_t toColumn,
               uint32_t color,
               uint32_t backgroundColor,
               int8_t position) {
  if (position < 0)
    return;
  int8_t rowStep = (toRow - fromRow) / 6;
  int8_t columnStep = (toColumn - fromColumn) / 6;
  for (uint8_t i = 0; i < 6; ++i) {
    uint32_t currentColor = i % 3 == position ? color : backgroundColor;
    pixels.setPixelColor(
        getLedIndex(fromRow + rowStep * i, fromColumn + columnStep * i),
        currentColor);
  }
}

/**
 * Should return a number between -1 and 3
 *
 * @param counter
 * @param power
 * @return uint8_t
 */
int8_t getFluxSpeed(uint16_t counter, float power) {
  int8_t powerLevel = round(power / 50);
  if (powerLevel <= 0)
    return -1;
  if (powerLevel > 100) {
    return (counter / 2) % 3;
  }
  if (powerLevel > 50) {
    return (counter / 5) % 3;
  }
  if (powerLevel > 20) {
    return (counter / 10) % 3;
  }
  return (counter / 25) % 3;
}

void froniusDisplay(Adafruit_NeoPixel& pixels, uint16_t counter) {
  pixels.clear();
  FroniusStatus status = getFroniusStatus();

  // PV
  uint8_t highValue = floor(status.powerFromPV / 500);
  uint8_t lowValue = floor((status.powerFromPV - highValue * 500) / 50);
  paintSquare(pixels, 0, 0, Adafruit_NeoPixel::Color(0xff, 0xff, 0x00),
              Adafruit_NeoPixel::Color(0x50, 0x50, 0x00),
              Adafruit_NeoPixel::Color(0x10, 0x10, 0x00), highValue, lowValue);
  // Battery

  paintSquare(pixels, 0, 11, Adafruit_NeoPixel::Color(0x00, 0xff, 0x00),
              Adafruit_NeoPixel::Color(0, 0, 0),
              Adafruit_NeoPixel::Color(0x00, 0x10, 0x00),
              (uint8_t)round((status.batteryChargePercentage - 6) / 5.68), 0);
  // Network
  float networkLevel = status.powerFromGrid;
  if (networkLevel < 0)
    networkLevel = -networkLevel;
  highValue = floor(networkLevel / 500);
  lowValue = floor((networkLevel - highValue * 500) / 50);
  paintSquare(pixels, 11, 0, Adafruit_NeoPixel::Color(0xff, 0xff, 0xff),
              Adafruit_NeoPixel::Color(0x50, 0x50, 0x50),
              Adafruit_NeoPixel::Color(0x10, 0x10, 0x10), highValue, lowValue);
  // Consumption
  highValue = floor(status.currentLoad / 500);
  lowValue = floor((status.currentLoad - highValue * 500) / 50);
  paintSquare(pixels, 11, 11, Adafruit_NeoPixel::Color(0xff, 0x00, 0x00),
              Adafruit_NeoPixel::Color(0x50, 0x00, 0x00),
              Adafruit_NeoPixel::Color(0x10, 0x00, 0x00), highValue, lowValue);

  // Flux Network to Consumption
  paintFlux(pixels, 13, 5, 13, 11, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromNetworkToLoad));

  // Flux PV to Network
  paintFlux(pixels, 5, 2, 11, 2, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromPVToNetwork));

  // Flux PV to Battery
  paintFlux(pixels, 2, 5, 2, 11, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromPVToBattery));

  // Flux PV to Consumption
  paintFlux(pixels, 5, 5, 11, 11, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromPVToLoad));

  // Flux Battery to Consumption
  paintFlux(pixels, 5, 13, 11, 13, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromBatteryToLoad));
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

void windDisplay(Adafruit_NeoPixel& pixels) {
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

void fullMeteoDisplay(Adafruit_NeoPixel& pixels) {
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
        uint16_t led = getLedIndex(15 - j - tens, i / 4 + 7);
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

#endif