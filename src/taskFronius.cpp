
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include "taskFronius.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "esp_http_client.h"

esp_err_t _http_event_handler(esp_http_client_event_t* evt);

char powerflow[1000] = {0};
StaticJsonDocument<1000> powerflowObject;

FroniusStatus froniusStatus;

FroniusStatus getFroniusStatus() {
  return froniusStatus;
}

esp_http_client_config_t configFronius = {
    .url = "http://192.168.1.187/status/powerflow",
    .method = HTTP_METHOD_GET,
    .timeout_ms = 2000,
    .event_handler =
        _http_event_handler,  // need an event handler to get the result
    .user_data = powerflow,
};

esp_http_client_handle_t clientFronius;
esp_err_t errFronius;

DeserializationError errorJSONFronius;

void printFroniusStatus(Print* output) {
  output->print("Power from battery: ");
  output->println(froniusStatus.powerFromBattery);
  output->print("Power from grid: ");
  output->println(froniusStatus.powerFromGrid);
  output->print("Current load: ");
  output->println(froniusStatus.currentLoad);
  output->print("Power from PV: ");
  output->println(froniusStatus.powerFromPV);
  output->print("Battery charge percentage: ");
  output->println(froniusStatus.batteryChargePercentage);
}

/*
  Update the weather forecast
*/
void TaskFronius(void* pvParameters) {
  vTaskDelay(10000);
  while (true) {
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(5000);
    }
    if (xSemaphoreTake(xSemaphoreFetch, 1) == pdTRUE) {
      clientFronius = esp_http_client_init(&configFronius);
      // GET
      errFronius = esp_http_client_perform(clientFronius);
      if (errFronius == ESP_OK) {
        if (strlen(powerflow) == 0) {
          Serial.println("No data from fronius");
        } else if (strlen(powerflow) > 900) {
          Serial.println("Data from fronius too long");
        } else {
          errorJSONFronius = deserializeJson(powerflowObject, powerflow);
          if (errorJSONFronius) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(errorJSONFronius.c_str());
          } else {
            froniusStatus.powerFromBattery =
                (float)powerflowObject["site"]["P_Akku"];
            froniusStatus.powerFromGrid =
                (float)powerflowObject["site"]["P_Grid"];
            froniusStatus.currentLoad =
                -(float)powerflowObject["site"]["P_Load"];
            froniusStatus.powerFromPV = (float)powerflowObject["site"]["P_PV"];
            froniusStatus.batteryChargePercentage =
                (float)powerflowObject["inverters"][0]["SOC"];

            // printFroniusStatus(&Serial);
          }
        }
      } else {
        Serial.println("Failing to retrieve fronius info");
      }
      esp_http_client_cleanup(clientFronius);
      xSemaphoreGive(xSemaphoreFetch);
    }
    vTaskDelay(2 * 1000);
  }
}

void taskFronius() {
  // Now set up two tasks to rntpdun independently.
  xTaskCreatePinnedToCore(TaskFronius, "TaskFronius",
                          20000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
