

struct FroniusStatus {
  float powerFromBattery;
  float powerFromGrid;
  float currentLoad;
  float powerFromPV;
  float batteryChargePercentage;
};

FroniusStatus getFroniusStatus();
