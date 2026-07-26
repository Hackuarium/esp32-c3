

#include <Arduino.h>

/*
  The live energy balance drawn by the LED wall, served by the solar monitoring
  backend (https://github.com/lpatiny/solar.patiny.com) on
  /api/energy-flow/compact.

  The backend aggregates the Fronius inverter AND the Marstek batteries and
  already splits the balance into source -> sink flows, so nothing is derived
  here. Every field maps one-to-one onto something the wall draws: four squares
  and the six links between them. Powers are in W and never negative; the battery
  level is usable Wh, with each pack's reserve floor already removed.
*/
struct FroniusStatus {
  // The four squares.
  float powerFromPV;      // "pv"
  float batteryStoredWh;    // "ba" — the whole fleet, BYD plus Marstek
  float bydStoredWh;        // "bd" — the BYD's share of it
  float marstekStoredWh;    // "mk" — the Marstek fleet's share of it
  float batteryCapacityWh;  // "bc" — usable capacity; the fleet is full near it
  float networkPower;     // "gr" — magnitude either way; the flux shows which
  float currentLoad;      // "co"
  // The six links.
  float fromPVToLoad;          // "ph"
  float fromPVToBattery;       // "pb"
  float fromPVToNetwork;       // "pg"
  float fromBatteryToLoad;     // "bh"
  float fromBatteryToNetwork;  // "bg"
  float fromNetworkToLoad;     // "gh"
  float fromNetworkToBattery;  // "gb"
  // True when the backend's own reading has aged out.
  bool isStale;
};

FroniusStatus getFroniusStatus();
void updateFronius();
