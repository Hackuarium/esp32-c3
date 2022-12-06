#include "./taskNTPD.h"
#include <WiFi.h>
// #include <sys/time.h>
#include "./common.h"
#include "./taskNTPD.h"
#include "esp_sntp.h"

const char* ntpServer = "pool.ntp.org";
// summer winter time: 7200 for summer, 3600 for winter
struct tm timeInfo;
time_t now;
char strftime_buf[64];

// TODO This task is useless and code can be moved to wifi
// can also add a paraemter for daylightOffset
void TaskNTPD(void* pvParameters) {
  setenv("TZ", "GMT-1", 1);  // -1 = winter, -2 = summer
  tzset();

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(5000);
  }

  while (true) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    vTaskDelay(60 * 60 * 1000);
  }
}

void printTime(Print* output) {
  time(&now);
  localtime_r(&now, &timeInfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeInfo);
  output->println(strftime_buf);
}

void getHourMinute(char* hourMinute) {
  time(&now);
  localtime_r(&now, &timeInfo);
  strftime(hourMinute, 6, "%H:%M", &timeInfo);
}

void getDayMonth(char* dayMonth) {
  time(&now);
  localtime_r(&now, &timeInfo);
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
  time(&now);
  localtime_r(&now, &timeInfo);
  return timeInfo.tm_hour * 60 + timeInfo.tm_min;
}

uint8_t getHour() {
  time(&now);
  localtime_r(&now, &timeInfo);
  return timeInfo.tm_hour;
}

int getSeconds() {
  time(&now);
  localtime_r(&now, &timeInfo);
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
