
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include "fronius.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "esp_http_client.h"
#include "http.h"

/*
  Live energy balance from the solar monitoring backend
  (https://github.com/lpatiny/solar.patiny.com). It aggregates the Fronius
  inverter AND the Marstek batteries, and already splits the balance into
  source -> sink flows, so this firmware no longer derives them from the raw
  Fronius powerflow. The response is ~400 bytes, well inside the 1000-byte fetch
  buffer. Override with -D ENERGY_FLOW_URL=... at build time.
*/
#ifndef ENERGY_FLOW_URL
#define ENERGY_FLOW_URL "http://192.168.1.30:60504/api/energy-flow"
#endif

StaticJsonDocument<1000> energyFlowObject;

FroniusStatus froniusStatus;

FroniusStatus getFroniusStatus() {
  return froniusStatus;
}

DeserializationError errorJSONFronius;

void printFroniusStatus(Print* output) {
  output->print("Power from PV: ");
  output->println(froniusStatus.powerFromPV);
  output->print("Current load: ");
  output->println(froniusStatus.currentLoad);
  output->print("Grid import: ");
  output->println(froniusStatus.gridImport);
  output->print("Grid export: ");
  output->println(froniusStatus.gridExport);
  output->print("Battery stored (Wh): ");
  output->println(froniusStatus.batteryStoredWh);
  output->print("Battery charge percentage: ");
  output->println(froniusStatus.batteryChargePercentage);
  output->print("From PV to load: ");
  output->println(froniusStatus.fromPVToLoad);
  output->print("From PV to battery: ");
  output->println(froniusStatus.fromPVToBattery);
  output->print("From PV to network: ");
  output->println(froniusStatus.fromPVToNetwork);
  output->print("From battery to load: ");
  output->println(froniusStatus.fromBatteryToLoad);
  output->print("From battery to network: ");
  output->println(froniusStatus.fromBatteryToNetwork);
  output->print("From network to load: ");
  output->println(froniusStatus.fromNetworkToLoad);
  output->print("From network to battery: ");
  output->println(froniusStatus.fromNetworkToBattery);
  output->println("");
}

/*
  Update the live energy balance
*/
void updateFronius() {
  char* energyFlow = fetch((char*)ENERGY_FLOW_URL);
  if (strlen(energyFlow) == 0) {
    Serial.println("No data from the solar backend");
    return;
  }
  errorJSONFronius = deserializeJson(energyFlowObject, energyFlow);
  if (errorJSONFronius) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(errorJSONFronius.c_str());
    return;
  }

  froniusStatus.powerFromPV = (float)energyFlowObject["production_w"];
  froniusStatus.currentLoad = (float)energyFlowObject["consumption_w"];
  froniusStatus.gridImport = (float)energyFlowObject["grid_import_w"];
  froniusStatus.gridExport = (float)energyFlowObject["grid_export_w"];
  froniusStatus.powerFromGrid =
      froniusStatus.gridImport - froniusStatus.gridExport;

  froniusStatus.batteryStoredWh = (float)energyFlowObject["battery_stored_wh"];
  froniusStatus.batteryCapacityWh =
      (float)energyFlowObject["battery_capacity_wh"];
  froniusStatus.batteryChargePercentage =
      (float)energyFlowObject["battery_soc_pct"];
  froniusStatus.batteryCharge = (float)energyFlowObject["battery_charge_w"];
  froniusStatus.batteryDischarge =
      (float)energyFlowObject["battery_discharge_w"];
  froniusStatus.powerFromBattery =
      froniusStatus.batteryDischarge - froniusStatus.batteryCharge;

  froniusStatus.fromPVToLoad = (float)energyFlowObject["solar_to_home_w"];
  froniusStatus.fromPVToBattery = (float)energyFlowObject["solar_to_battery_w"];
  froniusStatus.fromPVToNetwork = (float)energyFlowObject["solar_to_grid_w"];
  froniusStatus.fromBatteryToLoad =
      (float)energyFlowObject["battery_to_home_w"];
  froniusStatus.fromBatteryToNetwork =
      (float)energyFlowObject["battery_to_grid_w"];
  froniusStatus.fromNetworkToLoad = (float)energyFlowObject["grid_to_home_w"];
  froniusStatus.fromNetworkToBattery =
      (float)energyFlowObject["grid_to_battery_w"];

  froniusStatus.isStale = (bool)energyFlowObject["is_stale"];

  //  printFroniusStatus(&Serial);
}
