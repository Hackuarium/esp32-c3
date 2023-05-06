#include <Wire.h>
#include <stdio.h>
#include "./common.h"
#include "./params.h"

int16_t median(int16_t a[], int n);

void TaskRocket(void* pvParameters) {
  setParameter(PARAM_OUTPUT1, 0);
  setParameter(PARAM_OUTPUT2, 0);
  setParameter(PARAM_OUTPUT3, 0);
  setParameter(PARAM_SERVO1, 90);
  setParameter(PARAM_SERVO2, 90);
  setParameter(PARAM_SERVO3, 90);
  setParameter(PARAM_STATUS, 0);
  setParameter(PARAM_UP_DOWN_COUNTER, 0);

  vTaskDelay(1000);

  int16_t lastAltitudes[11] = {0};
  int16_t lastAltitudeIndex = 0;
  int16_t lastMedianAltitude = 0;

  int maxIncreased = 0;
  while (true) {
    vTaskDelay(50);

    int16_t upDownCounter = getParameter(PARAM_UP_DOWN_COUNTER);
    // we need to be 100% sure that the rocket has reached the apogee
    // before we start the parachute deployment sequence
    int16_t altitude = getParameter(PARAM_ALTITUDE);
    lastAltitudes[lastAltitudeIndex % 11] = altitude;
    lastAltitudeIndex++;

    int16_t medianAltitude = median(lastAltitudes, 11);

    if (medianAltitude > lastMedianAltitude) {
      upDownCounter++;
      if (getParameter(PARAM_UP_DOWN_COUNTER) > maxIncreased) {
        maxIncreased = upDownCounter;
      }
    } else if (medianAltitude < lastMedianAltitude) {
      upDownCounter--;
    }

    if ((maxIncreased - upDownCounter) > 10) {
      setParameter(PARAM_DEPLOY_PARACHUTE, 1);
    }

    setParameter(PARAM_UP_DOWN_COUNTER, upDownCounter);
    lastMedianAltitude = medianAltitude;
  }
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

void swap(int16_t* p, int16_t* q) {
  int t;
  t = *p;
  *p = *q;
  *q = t;
}

int16_t tempMedian[11] = {0};

int16_t median(int16_t a[], int n) {
  for (byte i = 0; i < n; i++) {
    tempMedian[i] = a[i];
  }
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (tempMedian[j] > tempMedian[j + 1])
        swap(&tempMedian[j], &tempMedian[j + 1]);
    }
  }

  return tempMedian[(n - 1) / 2];
}