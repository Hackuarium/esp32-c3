#include <WiFi.h>
#include "./params.h"

char ssid[20];
char password[20];

void TaskWifi(void* pvParameters) {
  vTaskDelay(2500);
  getParameter("wifi.ssid", ssid);
  getParameter("wifi.password", password);
  Serial.print("Trying to connect to ");
  Serial.println(ssid);
  Serial.print("Using password ");
  Serial.println(password);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  int counter = 0;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED && counter++ < 100) {
    vTaskDelay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  while (true) {
    vTaskDelay(1000);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WIFI disconnect, will restart");
      vTaskDelay(1000);
      ESP.restart();
      // WiFi.disconnect();
      // WiFi.begin(ssid, password);
      // WiFi.reconnect();
    }
    vTaskDelay(1000);
    setParameter(PARAM_WIFI_RSSI, WiFi.RSSI());
  }
}

void taskWifi() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskWifi, "TaskWifi",
                          12000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
