#include <WiFi.h>
#include "params.h"

#include "esp_wpa2.h"

char ssidAP[30];
char passwordAP[30];

void TaskWifiAP(void* pvParameters) {
  vTaskDelay(2500);

  while (getParameter(PARAM_WIFI_MODE) != 1) {
    vTaskDelay(1000);
  }

  getParameter("wifi.ap.ssid", ssidAP);
  if (strlen(ssidAP) == 0) {
    strcpy(ssidAP, WiFi.macAddress().c_str());
  }

  getParameter("wifi.ap.pass", passwordAP);
  if (strlen(passwordAP) == 0) {
    strcpy(passwordAP, "pass1234");
  }

  Serial.print("Trying to create wifi AP: ");
  Serial.println(ssidAP);
  Serial.print("Using password: ");
  Serial.println(passwordAP);

  WiFi.softAP(ssidAP, passwordAP);

  IPAddress IP = WiFi.softAPIP();

  Serial.print("AP IP address: ");
  Serial.println(IP);

  while (true) {
    vTaskDelay(1000);
  }
}

void taskWifiAP() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskWifiAP, "TaskWifiAP",
                          12000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          1,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
