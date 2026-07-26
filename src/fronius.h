

#include <Arduino.h>

/*
  Live energy balance served by the solar monitoring backend
  (https://github.com/lpatiny/solar.patiny.com) on /api/energy-flow.

  The backend already splits the balance into source -> sink flows, and folds the
  Marstek batteries into it, so nothing has to be derived here any more. Powers
  are in W and are never negative unless the name says otherwise; the battery
  level is in Wh.
*/
struct FroniusStatus {
  // The four displayed quantities.
  float powerFromPV;
  float currentLoad;
  float gridImport;
  float gridExport;
  float powerFromGrid;  // signed: positive importing, negative exporting
  float batteryStoredWh;
  float batteryCapacityWh;
  float batteryChargePercentage;
  float batteryCharge;
  float batteryDischarge;
  float powerFromBattery;  // signed: positive discharging, negative charging
  // The six links between them.
  float fromPVToLoad;
  float fromPVToBattery;
  float fromPVToNetwork;
  float fromBatteryToLoad;
  float fromBatteryToNetwork;
  float fromNetworkToLoad;
  float fromNetworkToBattery;
  // True when the backend's own reading has aged out.
  bool isStale;
};

FroniusStatus getFroniusStatus();
void updateFronius();
