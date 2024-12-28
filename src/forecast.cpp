

#include "forecast.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "http.h"

StaticJsonDocument<1000> forecastObject;

Forecast forecast = {.temperature = {0},
                     .precipitation = {0},
                     .windSpeed = {0},
                     .windDirection = {0},
                     .weather = {0},
                     .sunrise = "00:00",
                     .sunset = "00:00",
                     .sunriseInMin = 0,
                     .sunsetInMin = 0,
                     .nextIcon = 0};

DeserializationError errorJSONForecast;

/*
  Update the weather forecast
*/
void updateForecast() {
  char* responseForecast =
      fetch("http://weather-proxy.cheminfo.org/v2/forecast24");

  if (strlen(responseForecast) == 0) {
    Serial.println("No data from fronius");
  } else {
    errorJSONForecast = deserializeJson(forecastObject, responseForecast);
    if (errorJSONForecast) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(errorJSONForecast.c_str());
    } else {
      forecast.current.temperature =
          (int16_t)forecastObject["current"]["temperature"];
      forecast.current.precipitation =
          (float)forecastObject["current"]["precipitation"];
      forecast.current.windSpeed =
          (int16_t)forecastObject["current"]["windSpeed"];
      forecast.current.windDirection =
          (int16_t)forecastObject["current"]["windDirection"];
      forecast.current.weather = (int8_t)forecastObject["current"]["weather"];
      for (int i = 0; i < 8; i++) {
        forecast.temperature[i] = (int16_t)forecastObject["temperature"][i];
        forecast.precipitation[i] = (float)forecastObject["precipitation"][i];
        forecast.windSpeed[i] = (int16_t)forecastObject["windSpeed"][i];
        forecast.windDirection[i] = (int16_t)forecastObject["windDirection"][i];
        forecast.weather[i] = (int16_t)forecastObject["weather"][i];
      }
      strcpy(forecast.sunrise, forecastObject["sunrise"]);
      strcpy(forecast.sunset, forecastObject["sunset"]);
      forecast.sunriseInMin = getMinutes(forecast.sunrise);
      forecast.sunsetInMin = getMinutes(forecast.sunset);
      forecast.nextIcon = (int16_t)forecastObject["nextIcon"];
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

int16_t getMinutes(char* text) {
  int a = (text[0] - 48) * 10 + text[1] - 48;
  int b = (text[3] - 48) * 10 + text[4] - 48;
  return a * 60 + b;
}
