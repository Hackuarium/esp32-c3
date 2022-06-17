#include "./taskNTPD.h"
#include <WiFi.h>
#include <sys/time.h>
#include "./common.h"
#include "./taskNTPD.h"

const char* ntpServer = "pool.ntp.org";
// summer winter time: 7200 for summer, 3600 for winter
const long gmtOffset_sec = 7200;
const int daylightOffset_sec = 0;
struct tm timeInfo;

// TODO This task is useless and code can be moved to wifi
// can also add a paraemter for daylightOffset
void TaskNTPD(void* pvParameters) {
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(5000);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  while (true) {
    //    if(!getLocalTime(&timeInfo)){
    //      Serial.println("Failed to obtain time");
    //    }
    vTaskDelay(15 * 60 * 1000);
  }
}

void getHourMinute(char* hourMinute) {
  // todo we should use local time
  getLocalTime(&timeInfo);
  strftime(hourMinute, 6, "%H:%M", &timeInfo);
}

void getDayMonth(char* dayMonth) {
  getLocalTime(&timeInfo);
  strftime(dayMonth, 6, "%d-%m", &timeInfo);
}

void setEpoch(unsigned long epoch) {
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
}

unsigned long getEpoch() {
  time_t now;
  time(&now);
  return now;
}

int16_t getHourMinute() {
  getLocalTime(&timeInfo);
  return timeInfo.tm_hour * 60 + timeInfo.tm_min;
}

uint8_t getHour() {
  getLocalTime(&timeInfo);
  return timeInfo.tm_hour;
}

int getSeconds() {
  getLocalTime(&timeInfo);
  return timeInfo.tm_sec;
}

void taskNTPD() {
  // Now set up two tasks to rntpdun independently.
  xTaskCreatePinnedToCore(TaskNTPD, "TaskNTPD",
                          3000,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          1,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
