
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
  The LED wall payload from the solar monitoring backend: exactly the values the
  wall draws, under two-letter keys, in about 110 bytes. Served over plain HTTP
  from the public host, so the panel no longer needs to sit on the same LAN as
  the inverter. HTTP (not HTTPS) on purpose: a TLS handshake needs a ~40 KB
  contiguous heap block that this ESP32-S3 build (~55 KB free) cannot spare, so
  the device only ever does plain HTTP — same reason the weather forecast goes
  through an HTTP proxy.

  Override at build time with -D ENERGY_FLOW_URL=... to point at a local
  deployment.
*/
#ifndef ENERGY_FLOW_URL
#define ENERGY_FLOW_URL "http://solar.patiny.com/api/energy-flow/compact"
#endif

// 15 numeric members; 512 bytes is ample and leaves the parse allocation-free.
StaticJsonDocument<512> energyFlowObject;

FroniusStatus froniusStatus;

FroniusStatus getFroniusStatus() {
  return froniusStatus;
}

DeserializationError errorJSONFronius;

void printFroniusStatus(Print* output) {
  output->print("PV: ");
  output->println(froniusStatus.powerFromPV);
  output->print("Battery (Wh): ");
  output->print(froniusStatus.batteryStoredWh);
  output->print(" = BYD ");
  output->print(froniusStatus.bydStoredWh);
  output->print(" + Marstek ");
  output->print(froniusStatus.marstekStoredWh);
  output->print(" of ");
  output->println(froniusStatus.batteryCapacityWh);
  output->print("Network: ");
  output->println(froniusStatus.networkPower);
  output->print("Load: ");
  output->println(froniusStatus.currentLoad);
  output->print("PV -> load / battery / network: ");
  output->print(froniusStatus.fromPVToLoad);
  output->print(" / ");
  output->print(froniusStatus.fromPVToBattery);
  output->print(" / ");
  output->println(froniusStatus.fromPVToNetwork);
  output->print("Battery -> load / network: ");
  output->print(froniusStatus.fromBatteryToLoad);
  output->print(" / ");
  output->println(froniusStatus.fromBatteryToNetwork);
  output->print("Network -> load / battery: ");
  output->print(froniusStatus.fromNetworkToLoad);
  output->print(" / ");
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
  // A 503 (backend up, no inverter reading yet) parses fine but carries none of
  // these keys; keep the previous values rather than blanking the wall.
  if (!energyFlowObject.containsKey("pv")) {
    Serial.println("Solar backend has no reading yet");
    return;
  }

  froniusStatus.powerFromPV = (float)energyFlowObject["pv"];
  froniusStatus.batteryStoredWh = (float)energyFlowObject["ba"];
  // An older backend only reports the fleet total; book it all on the BYD so the
  // square keeps its single green instead of turning entirely Marstek mint.
  if (energyFlowObject.containsKey("bd")) {
    froniusStatus.bydStoredWh = (float)energyFlowObject["bd"];
    froniusStatus.marstekStoredWh = (float)energyFlowObject["mk"];
  } else {
    froniusStatus.bydStoredWh = froniusStatus.batteryStoredWh;
    froniusStatus.marstekStoredWh = 0;
  }
  // Zero when the backend does not report it; the square then never reads full.
  froniusStatus.batteryCapacityWh = (float)energyFlowObject["bc"];
  froniusStatus.networkPower = (float)energyFlowObject["gr"];
  froniusStatus.currentLoad = (float)energyFlowObject["co"];

  froniusStatus.fromPVToLoad = (float)energyFlowObject["ph"];
  froniusStatus.fromPVToBattery = (float)energyFlowObject["pb"];
  froniusStatus.fromPVToNetwork = (float)energyFlowObject["pg"];
  froniusStatus.fromBatteryToLoad = (float)energyFlowObject["bh"];
  froniusStatus.fromBatteryToNetwork = (float)energyFlowObject["bg"];
  froniusStatus.fromNetworkToLoad = (float)energyFlowObject["gh"];
  froniusStatus.fromNetworkToBattery = (float)energyFlowObject["gb"];

  froniusStatus.isStale = ((int)energyFlowObject["st"]) != 0;

  //  printFroniusStatus(&Serial);
}
