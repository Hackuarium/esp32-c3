#include "./common.h"
#include "./params.h"

void taskBlink();
void taskSi7021();
void taskSerial();
void taskNTPD();
void taskMQTT();
void taskWifi();
void taskOTA();


void setup() {
  Serial.begin(115200);  // only for debug purpose
  setupParameters();
  taskSerial();
  taskSi7021();
  taskOTA();
  taskWifi();
  taskMQTT();
  taskNTPD();
  taskBlink();

}

void loop() {
  vTaskDelay(30000);
}