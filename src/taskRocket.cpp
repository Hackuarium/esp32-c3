#include <Wire.h>
#include <stdio.h>
#include "./common.h"
#include "./params.h"

void initParameters();

void TaskRocket(void* pvParameters) {
  initParameters();
  setParameter(PARAM_STATUS, -1);

  vTaskDelay(3000);

  int16_t lastAltitude = 0;

  bool canSatReleased = false;
  int8_t numberConfirmationParachuteRelease = 0;

  int maxIncreased = 0;
  while (true) {
    vTaskDelay(50);
    if (getParameter(PARAM_STATUS) == -1) {
      initParameters();
      maxIncreased = 0;
      canSatReleased = false;
      numberConfirmationParachuteRelease = 0;
    }

    int16_t upDownCounter = getParameter(PARAM_UP_DOWN_COUNTER);
    // we need to be 100% sure that the rocket has reached the apogee
    // before we start the parachute deployment sequence
    int16_t altitude = getParameter(PARAM_RELATIVE_ALTITUDE);

    if (altitude > lastAltitude) {
      upDownCounter++;
      if (getParameter(PARAM_UP_DOWN_COUNTER) > maxIncreased) {
        maxIncreased = upDownCounter;
      }
    } else if (altitude < lastAltitude) {
      upDownCounter--;
    }

    if ((maxIncreased - upDownCounter) > 10) {
      setParameter(PARAM_SERVO1, getParameter(PARAM_SERVO1_ON));
      canSatReleased = true;
    }

    if (canSatReleased && ((getParameter(PARAM_ALTITUDE_PARACHUTE) -
                            getParameter(PARAM_RELATIVE_ALTITUDE)) > 0)) {
      numberConfirmationParachuteRelease++;
      if (numberConfirmationParachuteRelease > 3) {
        setParameter(PARAM_OUTPUT1, 1);
      }
    } else {
      numberConfirmationParachuteRelease = 0;
    }

    setParameter(PARAM_UP_DOWN_COUNTER, upDownCounter);
    lastAltitude = altitude;
  }
}

void initParameters() {
  if (getParameter(PARAM_SERVO1_OFF) == ERROR_VALUE) {
    setParameter(PARAM_SERVO1_OFF, 0);
    setParameter(PARAM_SERVO1_ON, 170);
  }
  setParameter(PARAM_OUTPUT1, 0);
  setParameter(PARAM_OUTPUT2, 0);
  setParameter(PARAM_OUTPUT3, 0);
  setParameter(PARAM_SERVO1, getParameter(PARAM_SERVO1_OFF));
  setParameter(PARAM_SERVO2, 90);
  setParameter(PARAM_SERVO3, 90);
  setParameter(PARAM_STATUS, 0);
  setParameter(PARAM_ALTITUDE_PARACHUTE, 200);
  setParameter(PARAM_UP_DOWN_COUNTER, 0);
}

void taskRocket() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskRocket, "TaskRocket",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
