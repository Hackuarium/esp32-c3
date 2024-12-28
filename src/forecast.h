#include "config.h"

struct Current {
  int16_t temperature;
  float precipitation;
  int16_t windSpeed;
  int16_t windDirection;
  int16_t weather;
};

struct Forecast {
  Current current;
  int16_t temperature[8];
  float precipitation[8];
  int16_t windSpeed[8];
  int16_t windDirection[8];
  int16_t weather[8];
  char sunrise[6];
  char sunset[6];
  int16_t sunriseInMin;
  int16_t sunsetInMin;
  int16_t nextIcon;
};

Forecast* getForecast();
void printForecast(Print* output);
void updateForecast();
int16_t getMinutes(char* text);