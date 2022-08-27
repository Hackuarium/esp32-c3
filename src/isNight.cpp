#include "./common.h"
#include "./params.h"
#include "taskForecast.h"
#include "taskNTPD.h"

bool isNight() {
  int16_t currentHourMinute = getHourMinute();
  return (((getSunriseInMin() + getParameter(PARAM_SUNRISE_OFFSET)) >
           currentHourMinute) ||
          ((getSunsetInMin() + getParameter(PARAM_SUNSET_OFFSET)) <
           currentHourMinute));
}