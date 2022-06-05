#include "./common.h"

void TaskNTPD(void* pvParameters);

void getHourMinute(char* hourMinute);
void getDayMonth(char* dayMonth);

void setEpoch(unsigned long epoch);

unsigned long getEpoch();
int16_t getHourMinute();
uint8_t getHour();
int getSeconds();

void taskNTPD();
