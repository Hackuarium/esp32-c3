

#include <Arduino.h>

#define MEDIAN_INT_SIZE 11

void swap(int16_t* p, int16_t* q) {
  int t;
  t = *p;
  *p = *q;
  *q = t;
}
int16_t tempIntMedian[MEDIAN_INT_SIZE] = {0};

int16_t getMedianInt11(int16_t a[]) {
  for (byte i = 0; i < MEDIAN_INT_SIZE; i++) {
    tempIntMedian[i] = a[i];
  }
  for (int i = 0; i < MEDIAN_INT_SIZE - 1; i++) {
    for (int j = 0; j < MEDIAN_INT_SIZE - i - 1; j++) {
      if (tempIntMedian[j] > tempIntMedian[j + 1])
        swap(&tempIntMedian[j], &tempIntMedian[j + 1]);
    }
  }

  return tempIntMedian[(MEDIAN_INT_SIZE - 1) / 2];
}