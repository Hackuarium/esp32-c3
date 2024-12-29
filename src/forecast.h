#include "config.h"

struct Current {
  int16_t temperature;
  float precipitation;
  int16_t windSpeed;
  int8_t windSpeedLog2;
  int16_t windDirection;
  uint8_t windDirection07;
  int8_t windDirectionColumn;
  int8_t windDirectionRow;
  int16_t weather;
  char windDirectionText[3];
};

struct Forecast {
  Current current;
  int16_t temperature[8];
  float precipitation[8];
  int16_t windSpeed[8];
  uint8_t windSpeedLog2[8];
  int16_t windDirection[8];
  uint8_t windDirection07[8];
  int16_t weather[8];
  char sunrise[6];
  char sunset[6];
  float lunarAge;
  int16_t sunriseInMin;
  int16_t sunsetInMin;
  int16_t nextIcon;
};

Forecast* getForecast();
void printForecast(Print* output);
void updateForecast();
int16_t getMinutes(char* text);

int8_t getWindColumnDirection(uint8_t windDirection07);
int8_t getWindRowDirection(uint8_t windDirection07);