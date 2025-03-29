#include "config.h"
#include "isNight.h"
#include "params.h"

const uint8_t ACTIONS[8] = {PARAM_ACTION_1, PARAM_ACTION_2, PARAM_ACTION_3,
                            PARAM_ACTION_4, PARAM_ACTION_5, PARAM_ACTION_6,
                            PARAM_ACTION_7, PARAM_ACTION_8};

void printSchedule(Print* output) {
  for (uint8_t i = 0; i < sizeof(ACTIONS); i++) {
    int16_t currentActionValue = getParameter(ACTIONS[i]);
    if (currentActionValue == -1) {
      continue;
    }
    int16_t brightness = currentActionValue & 0xFF;
    int16_t quarters = currentActionValue >> 8;
    output->print("Action ");
    output->print(i + 1);
    output->print(" : ");
    output->printf("%02i", quarters / 4);
    output->print(":");
    output->printf("%02i", (quarters % 4) * 15);
    output->print(" - ");
    output->println(brightness);
  }
  output->print("Is night: ");
  output->println(isNight());
  output->print("On during the day: ");
  output->println(getParameterBit(PARAM_SCHEDULE, 0));
  output->print("On during the night: ");
  output->println(getParameterBit(PARAM_SCHEDULE, 1));
  output->print("Sunset offset: ");
  output->println(getParameter(PARAM_SUNSET_OFFSET));
  output->print("Sunrise offset: ");
  output->println(getParameter(PARAM_SUNRISE_OFFSET));
}