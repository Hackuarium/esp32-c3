

#include <Arduino.h>
struct FroniusStatus {
  float powerFromBattery;
  float powerFromGrid;
  float currentLoad;
  float powerFromPV;
  float batteryChargePercentage;
  float fromNetworkToLoad;
  float fromPVToNetwork;
  float fromPVToBattery;
  float fromPVToLoad;
  float fromBatteryToLoad;
};

FroniusStatus getFroniusStatus();
void updateFronius();
