
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include "fronius.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "esp_http_client.h"
#include "http.h"

StaticJsonDocument<1000> powerflowObject;

FroniusStatus froniusStatus;

FroniusStatus getFroniusStatus() {
  return froniusStatus;
}

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
  output->print("From network to load: ");
  output->println(froniusStatus.fromNetworkToLoad);
  output->print("From PV to network: ");
  output->println(froniusStatus.fromPVToNetwork);
  output->print("From PV to battery: ");
  output->println(froniusStatus.fromPVToBattery);
  output->print("From PV to load: ");
  output->println(froniusStatus.fromPVToLoad);
  output->print("From battery to load: ");
  output->println(froniusStatus.fromBatteryToLoad);
  output->println("");
}

/*
  Update the weather forecast
*/
void updateFronius() {
  char* powerflow = fetch("http://192.168.1.30/status/powerflow");
  if (strlen(powerflow) == 0) {
    Serial.println("No data from fronius");
  } else {
    errorJSONFronius = deserializeJson(powerflowObject, powerflow);
    if (errorJSONFronius) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(errorJSONFronius.c_str());
    } else {
      froniusStatus.powerFromBattery = (float)powerflowObject["site"]["P_Akku"];
      froniusStatus.powerFromGrid = (float)powerflowObject["site"]["P_Grid"];
      froniusStatus.currentLoad = -(float)powerflowObject["site"]["P_Load"];
      froniusStatus.powerFromPV = (float)powerflowObject["site"]["P_PV"];
      froniusStatus.batteryChargePercentage =
          (float)powerflowObject["inverters"][0]["SOC"];

      if (froniusStatus.powerFromBattery > 0) {
        froniusStatus.fromBatteryToLoad = froniusStatus.powerFromBattery;
        froniusStatus.fromPVToBattery = 0;
      } else {
        froniusStatus.fromBatteryToLoad = 0;
        froniusStatus.fromPVToBattery = -froniusStatus.powerFromBattery;
      }

      if (froniusStatus.powerFromGrid > 0) {
        froniusStatus.fromNetworkToLoad = froniusStatus.powerFromGrid;
        froniusStatus.fromPVToNetwork = 0;
      } else {
        froniusStatus.fromNetworkToLoad = 0;
        froniusStatus.fromPVToNetwork = -froniusStatus.powerFromGrid;
      }

      froniusStatus.fromPVToLoad = froniusStatus.powerFromPV -
                                   froniusStatus.fromPVToNetwork -
                                   froniusStatus.fromPVToBattery;

      //  printFroniusStatus(&Serial);
    }
  }
}
