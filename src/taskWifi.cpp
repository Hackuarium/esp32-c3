#include <WiFi.h>
#include "./params.h"
#include "esp_wpa2.h"

char ssid[30];
char password[30];
// the wifi also requires username and password
char username[30];
char identity[30];

void TaskWifi(void* pvParameters) {
  vTaskDelay(2500);
  getParameter("wifi.ssid", ssid);
  getParameter("wifi.password", password);
  getParameter("wifi.username", username);
  getParameter("wifi.identity", identity);

  Serial.print("Trying to connect to ");
  Serial.println(ssid);
  Serial.print("Using password ");
  Serial.println(password);

  WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
 // WiFi.disconnect(true);
        //
// if identity not defined use ursername as identity
  if (strlen(identity) == 0) {
    strcpy(identity, username);
  }
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)identity, strlen(identity));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();
  /**
    int counter = 0;
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED && counter++ < 100) {
      vTaskDelay(500);
      Serial.print(".");
    }
    **/

  /**
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    **/
  while (true) {
    vTaskDelay(1000);
    byte counter = 0;
    while (WiFi.status() != WL_CONNECTED && counter++ < 10) {
      setParameter(PARAM_WIFI_RSSI, -1);
      Serial.println("WIFI not connected, trying to connect");
  

  
      WiFi.begin(ssid);
      // WiFi.reconnect();
      vTaskDelay(5000);
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WIFI connected");
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      setParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_WIFI);
    } else {
      clearParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_WIFI);
      setParameter(PARAM_WIFI_RSSI, WiFi.RSSI());
    }
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


