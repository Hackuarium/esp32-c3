#include "config.h"
#ifdef PARAM_SUNRISE_OFFSET

#include "forecast.h"
#include "params.h"
#include "taskNTPD.h"

bool isNight() {
  int16_t currentHourMinute = getHourMinute();
  Forecast* forecast = getForecast();
  return (((forecast->sunriseInMin + getParameter(PARAM_SUNRISE_OFFSET)) >
           currentHourMinute) ||
          ((forecast->sunsetInMin + getParameter(PARAM_SUNSET_OFFSET)) <
           currentHourMinute));
}

#endif