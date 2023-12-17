#include "config.h"
#include "./params.h"
#include "taskNTPD.h"
uint8_t ACTIONS[8] = {PARAM_ACTION_1, PARAM_ACTION_2, PARAM_ACTION_3,
                      PARAM_ACTION_4, PARAM_ACTION_5, PARAM_ACTION_6,
                      PARAM_ACTION_7, PARAM_ACTION_8};

void doAction() {
  int16_t currentHourMinute = getHourMinute();
  for (uint8_t i = 0; i < sizeof(ACTIONS); i++) {
    int16_t currentActionValue = getParameter(ACTIONS[i]);
    if (currentActionValue < 0) {
      continue;
    }
    int16_t brightness = currentActionValue / 2000;
    int16_t hourMinute = currentActionValue - brightness * 2000;
    /*
        Serial.print(currentHourMinute);
        Serial.print(" ");
        Serial.print(brightness);
        Serial.print(" ");
        Serial.println(hourMinute);
    */
    if (currentHourMinute == hourMinute) {
      brightness *= 16;
      if (brightness > 230)
        brightness = 255;
      if (getParameter(PARAM_BRIGHTNESS) != brightness) {
        setAndSaveParameter(PARAM_BRIGHTNESS, brightness);
      }
    }
  }
}