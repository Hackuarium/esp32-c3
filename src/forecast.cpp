

#include "forecast.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "http.h"

uint8_t getWindDirection07(uint16_t windDirection);
void setWindDirectionText(uint8_t windDirection, char* windDirectionText);

StaticJsonDocument<1500> forecastObject;

Forecast forecast = {.temperature = {0},
                     .precipitation = {0},
                     .windSpeed = {0},
                     .windDirection = {0},
                     .weather = {0},
                     .sunrise = "00:00",
                     .sunset = "00:00",
                     .sunriseInMin = 0,
                     .sunsetInMin = 0,
                     .nextIcon = 0,
                     .dayMonth = "00-00"};

DeserializationError errorJSONForecast;

/*
  Update the weather forecast
*/
void updateForecast() {
  char* responseForecast =
      fetch("http://weather-proxy.cheminfo.org/v2/forecast24");

  if (strlen(responseForecast) == 0) {
    Serial.println("No data from forecast server");
  } else {
    errorJSONForecast = deserializeJson(forecastObject, responseForecast);
    if (errorJSONForecast) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(errorJSONForecast.c_str());
    } else {
      float airTemperature = (float)forecastObject["current"]["temperature"];
      float airFeltTemperature =
          forecastObject["current"]["feltTemperature"] | airTemperature;
      JsonObject outside = forecastObject["outside"];
      if (outside.containsKey("temperature")) {
        float outsideTemperature = (float)outside["temperature"];
        forecast.current.temperature = (int16_t)round(outsideTemperature);
        /* The proxy computes the felt temperature on the local sensor whenever
           it has one. Without it, the felt temperature is only meaningful
           relative to the air temperature it was computed from, so carry over
           the delta instead of the absolute value: the displayed base then comes
           from the local sensor, which reads several degrees apart from the
           forecast. */
        forecast.current.feltTemperature =
            outside.containsKey("feltTemperature")
                ? (int16_t)round((float)outside["feltTemperature"])
                : (int16_t)round(outsideTemperature + airFeltTemperature -
                                 airTemperature);
      } else {
        forecast.current.temperature = (int16_t)round(airTemperature);
        forecast.current.feltTemperature = (int16_t)round(airFeltTemperature);
      }
      forecast.current.precipitation =
          (float)forecastObject["current"]["precipitation"];
      forecast.current.windSpeed =
          round((float_t)forecastObject["current"]["windSpeed"]);
      forecast.current.windSpeedLog2 = floor(log2(forecast.current.windSpeed));
      forecast.current.windDirection =
          (int16_t)forecastObject["current"]["windDirection"];
      forecast.current.gustSpeed =
          round((float_t)forecastObject["current"]["gustSpeed"]);
      forecast.current.windDirection07 =
          getWindDirection07(forecast.current.windDirection);
      forecast.current.windDirectionColumn =
          getWindColumnDirection(forecast.current.windDirection07);
      forecast.current.windDirectionRow =
          getWindRowDirection(forecast.current.windDirection07);

      setWindDirectionText(forecast.current.windDirection07,
                           forecast.current.windDirectionText);
      forecast.current.weather = (int8_t)forecastObject["current"]["weather"];
      for (int i = 0; i < 8; i++) {
        forecast.temperature[i] = (int16_t)forecastObject["temperature"][i];
        forecast.precipitation[i] = (float)forecastObject["precipitation"][i];
        forecast.windSpeed[i] = (int16_t)forecastObject["windSpeed"][i];
        forecast.gustSpeed[i] = (int16_t)forecastObject["gustSpeed"][i];
        forecast.windSpeedLog2[i] = floor(log2(forecast.windSpeed[i]));
        forecast.windDirection[i] = (int16_t)forecastObject["windDirection"][i];
        forecast.windDirection07[i] =
            getWindDirection07(forecast.windDirection[i]);
        forecast.weather[i] = (int16_t)forecastObject["weather"][i];
      }
      strcpy(forecast.sunrise, forecastObject["sunrise"]);
      strcpy(forecast.sunset, forecastObject["sunset"]);
      forecast.sunriseInMin = getMinutes(forecast.sunrise);
      forecast.sunsetInMin = getMinutes(forecast.sunset);
      forecast.nextIcon = (int16_t)forecastObject["nextIcon"];
      forecast.lunarAge = (float)forecastObject["lunarAge"];
      strcpy(forecast.dayMonth, forecastObject["dayMonth"]);
    }
  }
}

void printForecast(Print* output) {
  output->print("Temperature: ");
  output->println(forecast.current.temperature, 0);
  output->print("Precipitation: ");
  output->println(forecast.current.precipitation);
  output->print("Wind speed: ");
  output->println(forecast.current.windSpeed, 0);
  output->print("Wind direction: ");
  output->println(forecast.current.windDirection, 0);
  output->print("Weather icon: ");
  output->println(forecast.current.weather);
  output->print("Lunar age: ");
  output->println(forecast.lunarAge);
  output->println("Next 24 hours per slot of 3h:");
  output->print("- temperature: ");
  for (int i = 0; i < 8; i++) {
    output->print(forecast.temperature[i]);
    if (i < 7) {
      output->print(" ");
    }
  }
  output->println();
  output->print("- precipitation: ");
  for (int i = 0; i < 8; i++) {
    output->print(forecast.precipitation[i]);
    if (i < 7) {
      output->print(" ");
    }
  }
  output->println();
  output->print("- wind speed: ");
  for (int i = 0; i < 8; i++) {
    output->print(forecast.windSpeed[i], 0);
    if (i < 7) {
      output->print(" ");
    }
  }
  output->println();
  output->print("- wind direction: ");
  for (int i = 0; i < 8; i++) {
    output->print(forecast.windDirection[i]);
    if (i < 7) {
      output->print(" ");
    }
  }
  output->println();
  output->print("Sunrise: ");
  output->println(forecast.sunrise);
  output->print("Sunset: ");
  output->println(forecast.sunset);
}

Forecast* getForecast() {
  return &forecast;
}

int8_t getWindColumnDirection(uint8_t windDirection07) {
  switch (windDirection07) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      return 1;
    case 3:
      return 1;
    case 4:
      return 0;
    case 5:
      return -1;
    case 6:
      return -1;
    case 7:
      return -1;
    default:
      return 0;
  }
}

int8_t getWindRowDirection(uint8_t windDirection07) {
  switch (windDirection07) {
    case 0:
      return -1;
    case 1:
      return -1;
    case 2:
      return 0;
    case 3:
      return 1;
    case 4:
      return 1;
    case 5:
      return 1;
    case 6:
      return 0;
    case 7:
      return -1;
    default:
      return 0;
  }
}

int16_t getMinutes(char* text) {
  int a = (text[0] - 48) * 10 + text[1] - 48;
  int b = (text[3] - 48) * 10 + text[4] - 48;
  return a * 60 + b;
}

uint8_t getWindDirection07(uint16_t windDirection) {
  return floor(((windDirection + 22) % 360) / 45);
}

void setWindDirectionText(uint8_t windDirection07, char* windDirectionText) {
  switch (windDirection07) {
    case 0:
      strcpy(windDirectionText, "N");
      break;
    case 1:
      strcpy(windDirectionText, "NE");
      break;
    case 2:
      strcpy(windDirectionText, "E");
      break;
    case 3:
      strcpy(windDirectionText, "SE");
      break;
    case 4:
      strcpy(windDirectionText, "S");
      break;
    case 5:
      strcpy(windDirectionText, "SW");
      break;
    case 6:
      strcpy(windDirectionText, "W");
      break;
    case 7:
      strcpy(windDirectionText, "NW");
      break;
    default:
      strcpy(windDirectionText, "NA");
  }
}