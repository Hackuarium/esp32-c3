

#include <Arduino.h>

#define MEDIAN_FLOAT_SIZE 11

void swap(float* p, float* q) {
  float t;
  t = *p;
  *p = *q;
  *q = t;
}
float tempFloatMedian[MEDIAN_FLOAT_SIZE] = {0};

float getMedianFloat11(float a[]) {
  for (byte i = 0; i < MEDIAN_FLOAT_SIZE; i++) {
    tempFloatMedian[i] = a[i];
  }
  for (int i = 0; i < MEDIAN_FLOAT_SIZE - 1; i++) {
    for (int j = 0; j < MEDIAN_FLOAT_SIZE - i - 1; j++) {
      if (tempFloatMedian[j] > tempFloatMedian[j + 1])
        swap(&tempFloatMedian[j], &tempFloatMedian[j + 1]);
    }
  }

  return tempFloatMedian[(MEDIAN_FLOAT_SIZE - 1) / 2];
}