#include "config.h"
#ifdef PARAM_SUNRISE_OFFSET

#include "forecast.h"
#include "params.h"
#include "taskNTPD.h"

bool isNight() {
  int16_t currentHourMinute = getHourMinute();
  return (((getSunriseInMin() + getParameter(PARAM_SUNRISE_OFFSET)) >
           currentHourMinute) ||
          ((getSunsetInMin() + getParameter(PARAM_SUNSET_OFFSET)) <
           currentHourMinute));
}

#endif