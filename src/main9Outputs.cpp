#include "config.h"
#if BOARD_TYPE == KIND_9_OUTPUTS
#include "params.h"

void deepSleep(int seconds);
void taskBlink();
void taskSerial();
void taskNTPD();
void taskPixels();
void taskOutput();
void taskOneWire();
void taskTest();
void taskFetch();
void taskWifi();
void taskWifiAP();
void taskOTA();
void taskMDNS();
void taskInternalTemperature();
void taskWebserver();
void task9Outputs();
void taskInput();
void taskRestart();
void taskTimer();
void taskSchedule();

// UPDATE using /rgb5.html
void setup9Outputs() {
  Serial.begin(115200);  // only for debug purpose
  setParameter(PARAM_STATUS, 0);

  setupParameters();
  // taskTest();

  taskSerial();

  // taskInput();
  task9Outputs();
  // taskRestart();
  taskOTA();
  // taskMDNS();  // incompatible with taskOTA
  taskWifi();

  taskFetch();

  // taskWifiAP();
  taskWebserver();
  // taskTimer();
#ifdef THR_SCHEDULE
  taskSchedule();
#endif
  taskNTPD();
  taskInternalTemperature();

  vTaskDelay(5 * 1000);  // waiting 30s before normal operation
}

void loop9Outputs() {
  vTaskDelay(100);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  int16_t highBrightness = 128;
  int16_t lowBrightness = 32;

  setAndSaveParameter(PARAM_SPEED, 1);
  setAndSaveParameter(PARAM_BRIGHTNESS, highBrightness);

  setAndSaveParameter(PARAM_OUT1_COLOR1, 3840);
  setAndSaveParameter(PARAM_OUT1_COLOR2, 3855);
  setAndSaveParameter(PARAM_OUT1_COLOR3, 527);
  setAndSaveParameter(PARAM_OUT1_COLOR4, 175);
  setAndSaveParameter(PARAM_OUT1_COLOR5, 251);
  setAndSaveParameter(PARAM_OUT1_COLOR6, 1776);
  setAndSaveParameter(PARAM_OUT1_COLOR7, 4048);
  setAndSaveParameter(PARAM_OUT1_COLOR8, 3952);
  setAndSaveParameter(PARAM_OUT2_COLOR1, 0);
  setAndSaveParameter(PARAM_OUT2_COLOR2, 0);
  setAndSaveParameter(PARAM_OUT2_COLOR3, 0);
  setAndSaveParameter(PARAM_OUT2_COLOR4, 0);
  setAndSaveParameter(PARAM_OUT2_COLOR5, 0);
  setAndSaveParameter(PARAM_OUT2_COLOR6, 0);
  setAndSaveParameter(PARAM_OUT2_COLOR7, 0);
  setAndSaveParameter(PARAM_OUT2_COLOR8, 0);

  setAndSaveParameter(PARAM_SCHEDULE, 2);
  // Full power in the evening but reduce at 10PM and
  // turn off at 12PM Turn on in the morning at 5AM but not too strong
  setAndSaveParameter(PARAM_SUNSET_OFFSET, -30);
  setAndSaveParameter(PARAM_SUNRISE_OFFSET, 30);
  setAndSaveParameter(
      PARAM_ACTION_1,
      ((4 * 16) << 8) + highBrightness);  // intensity high, at 16:00 but anyway
                                          // we only turn on during the night
  setAndSaveParameter(
      PARAM_ACTION_2,
      ((4 * 22) << 8) + lowBrightness);    // intensity low, at 22:00
  setAndSaveParameter(PARAM_ACTION_3, 0);  // turn off at 00:00
  setAndSaveParameter(
      PARAM_ACTION_4,
      ((4 * 5) << 8) + lowBrightness);  // intensity low, at 5:00
  setAndSaveParameter(
      PARAM_ACTION_5,
      ((4 * 7 + 2) << 8) + highBrightness);  // intensity high, at 7:30 = CI7712
  setAndSaveParameter(PARAM_ACTION_6,
                      ((4 * 9) << 8) + 0);  // intensity off, at 9:00 = CJ9216
  setAndSaveParameter(PARAM_ACTION_7, -1);
  setAndSaveParameter(PARAM_ACTION_8, -1);

  setAndSaveParameter(PARAM_COLOR_TRANSITION_NB_STEPS,
                      25);  // transition speed is 1s
}

#endif