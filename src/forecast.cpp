
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include <WiFi.h>
#include "./charToFloat.h"
#include "config.h"
#include "http.h"

static float_t forecast[36] = {0};
static float_t currentWeather[2] = {NAN};  // temperature + humidity
static char sunrise[6] = {0};
static char sunset[6] = {0};
static int16_t sunriseMin = 0;
static int16_t sunsetMin = 0;

int16_t getMinutes(char* text);

/*
  Update the weather forecast
*/
void updateForecast() {
  static char* token;
  char* responseForecast =
      fetch("http://weather-proxy.cheminfo.org/forecast24");

  if (strlen(responseForecast) > 0) {
    currentWeather[2] = {NAN};
    // we will parse the data
    token = strtok(responseForecast, ",");
    uint8_t position = 0;

    while (token != NULL) {
      if (position == 37) {
        position++;
        for (int i = 0; i < 5; i++) {
          sunset[i] = token[i];
        }
        sunsetMin = getMinutes(sunset);
      } else if (position == 36) {
        position++;
        for (int i = 0; i < 5; i++) {
          sunrise[i] = token[i];
        }
        sunriseMin = getMinutes(sunrise);
      } else if (position < 36) {
        forecast[position] = charToFloat(token);
        position++;
      } else if (position > 37 && position < 40) {
        currentWeather[position - 38] = charToFloat(token);
        position++;
      }
      token = strtok(NULL, ",");
    }
  }
}

void printSunrise(Print* output) {
  output->print("Sunrise: ");
  output->println(sunrise);
}

void printSunset(Print* output) {
  output->print("Sunset: ");
  output->println(sunset);
}

float_t* getForecast() {
  return forecast;
}

float_t* getCurrentWeather() {
  return currentWeather;
}

char* getSunrise() {
  return sunrise;
}

int16_t getSunriseInMin() {
  return sunriseMin;
}

int16_t getSunsetInMin() {
  return sunsetMin;
}

char* getSunset() {
  return sunset;
}

int16_t getMinutes(char* text) {
  int a = (text[0] - 48) * 10 + text[1] - 48;
  int b = (text[3] - 48) * 10 + text[4] - 48;
  return a * 60 + b;
}
