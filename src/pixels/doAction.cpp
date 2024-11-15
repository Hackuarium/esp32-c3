#include "config.h"
#ifdef MAX_LED

#include "params.h"
#include "taskNTPD.h"

const uint8_t ACTIONS[8] = {PARAM_ACTION_1, PARAM_ACTION_2, PARAM_ACTION_3,
                            PARAM_ACTION_4, PARAM_ACTION_5, PARAM_ACTION_6,
                            PARAM_ACTION_7, PARAM_ACTION_8};

void doAction() {
  int16_t currentHourMinute = getHourMinute();
  for (uint8_t i = 0; i < sizeof(ACTIONS); i++) {
    int16_t currentActionValue = getParameter(ACTIONS[i]);
    if (currentActionValue < 0) {
      continue;
    }
    int16_t brightness = currentActionValue & 0xFF;
    int16_t hourMinute = (currentActionValue >> 8) * 15;
    if (currentHourMinute == hourMinute) {
      if (getParameter(PARAM_BRIGHTNESS) != brightness) {
        setAndSaveParameter(PARAM_BRIGHTNESS, brightness);
      }
    }
  }
}

#endif