
#include <Adafruit_NeoPixel.h>
#include "../pixels.h"
#include "config.h"
#include "fronius.h"

struct SquareColors {
  uint32_t high;        // one perimeter LED: a coarse unit
  uint32_t low;         // one centre LED: a fine unit
  uint32_t background;  // unlit
};

/*
  A quantity square. `upper` splits it between two stacked sources: the first
  `lowerLeds` perimeter LEDs keep the lower source's colour and the rest take the
  upper one's, which is how the battery square tells the BYD apart from the
  Marstek fleet. The centre sits at the top of the stack, so `upperHoldsCentre`
  says which of the two owns the remainder. Pass nullptr for a single source.

  `full` lights the whole square at full brightness — the top of the scale. A
  power square has no known maximum, so it says so by overflowing the ring; the
  battery knows its usable capacity and passes the flag instead, which lets it
  run out of LEDs (it holds more than the 16.9 kWh they cover) without ever
  claiming to be full when it is not.
*/
void paintSquare(Adafruit_NeoPixel& pixels,
                 uint8_t row,
                 uint8_t column,
                 const SquareColors& colors,
                 uint8_t highValue,
                 uint8_t lowValue,
                 bool full = false,
                 const SquareColors* upper = nullptr,
                 uint8_t lowerLeds = 0,
                 bool upperHoldsCentre = false) {
  bool complete = full || highValue > 16;
  if (complete) {
    highValue = 16;
    lowValue = 9;
  }

  static uint8_t rowLevels[16] = {0, 0, 0, 0, 0, 1, 2, 3,
                                  4, 4, 4, 4, 4, 3, 2, 1};
  static uint8_t columnLevels[16] = {0, 1, 2, 3, 4, 4, 4, 4,
                                     4, 3, 2, 1, 0, 0, 0, 0};

  for (uint8_t i = 0; i < 5; ++i) {
    for (uint8_t j = 0; j < 5; ++j) {
      pixels.setPixelColor(getLedIndex(row + i, column + j),
                           colors.background);
    }
  }

  for (uint8_t i = 0; i < highValue; ++i) {
    uint32_t ledColor =
        (upper != nullptr && i >= lowerLeds) ? upper->high : colors.high;
    pixels.setPixelColor(
        getLedIndex(row + rowLevels[i], column + columnLevels[i]), ledColor);
  }

  // details information in the centrum
  const SquareColors& centre =
      (upper != nullptr && upperHoldsCentre) ? *upper : colors;
  uint32_t centreColor = complete ? centre.high : centre.low;
  for (uint8_t i = 0; i < min(lowValue, (uint8_t)9); ++i) {
    pixels.setPixelColor(
        getLedIndex(row + 1 + floor(i / 3), column + 1 + i % 3), centreColor);
  }
}

void paintFlux(Adafruit_NeoPixel& pixels,
               uint8_t fromRow,
               uint8_t fromColumn,
               uint8_t toRow,
               uint8_t toColumn,
               uint32_t color,
               uint32_t backgroundColor,
               int8_t position) {
  if (position < 0)
    return;
  int8_t rowStep = (toRow - fromRow) / 6;
  int8_t columnStep = (toColumn - fromColumn) / 6;
  for (uint8_t i = 0; i < 6; ++i) {
    uint32_t currentColor = i % 3 == position ? color : backgroundColor;
    pixels.setPixelColor(
        getLedIndex(fromRow + rowStep * i, fromColumn + columnStep * i),
        currentColor);
  }
}

/**
 * Should return a number between -1 and 3
 *
 * @param counter
 * @param power
 * @return uint8_t
 */
int8_t getFluxSpeed(uint16_t counter, float power) {
  int8_t powerLevel = round(power / 50);
  if (powerLevel <= 0)
    return -1;
  if (powerLevel > 100) {
    return (counter / 2) % 3;
  }
  if (powerLevel > 50) {
    return (counter / 5) % 3;
  }
  if (powerLevel > 20) {
    return (counter / 10) % 3;
  }
  return (counter / 25) % 3;
}

void froniusDisplay(Adafruit_NeoPixel& pixels, uint16_t counter) {
  pixels.clear();
  FroniusStatus status = getFroniusStatus();

  const SquareColors solarColors = {Adafruit_NeoPixel::Color(0xff, 0xff, 0x00),
                                    Adafruit_NeoPixel::Color(0x50, 0x50, 0x00),
                                    Adafruit_NeoPixel::Color(0x10, 0x10, 0x00)};
  // The two greens of the battery square: pure green for the Fronius BYD,
  // turquoise for the Marstek fleet stacked on top of it. The two hues are far
  // apart on purpose — the dim centre LEDs have no neighbour of the other colour
  // to be judged against, so a close pair reads as a single green.
  const SquareColors bydColors = {Adafruit_NeoPixel::Color(0x00, 0xff, 0x00),
                                  Adafruit_NeoPixel::Color(0x00, 0x50, 0x00),
                                  Adafruit_NeoPixel::Color(0x00, 0x10, 0x00)};
  const SquareColors marstekColors = {
      Adafruit_NeoPixel::Color(0x00, 0xff, 0xd0),
      Adafruit_NeoPixel::Color(0x00, 0x50, 0x41),
      Adafruit_NeoPixel::Color(0x00, 0x10, 0x0d)};
  const SquareColors networkColors = {
      Adafruit_NeoPixel::Color(0xff, 0xff, 0xff),
      Adafruit_NeoPixel::Color(0x50, 0x50, 0x50),
      Adafruit_NeoPixel::Color(0x10, 0x10, 0x10)};
  const SquareColors loadColors = {Adafruit_NeoPixel::Color(0xff, 0x00, 0x00),
                                   Adafruit_NeoPixel::Color(0x50, 0x00, 0x00),
                                   Adafruit_NeoPixel::Color(0x10, 0x00, 0x00)};

  // PV: 500 W per LED around, 50 W per LED inside
  uint8_t highValue = floor(status.powerFromPV / 500);
  uint8_t lowValue = floor((status.powerFromPV - highValue * 500) / 50);
  paintSquare(pixels, 0, 0, solarColors, highValue, lowValue);

  // Battery: the USABLE energy stored across every pack (Fronius BYD plus the
  // Marstek units) — the backend already subtracts each pack's reserve floor, so
  // the square goes dark when the fleet is empty rather than keeping a slice
  // permanently lit. 1 kWh per LED around and 100 Wh per LED inside, which the
  // ~18.4 kWh fleet overruns: past 16.9 kWh the square is simply maxed out. It
  // lights up bright only within 1 % of the usable capacity the backend reports,
  // so "everything lit" means genuinely full and not merely off the scale.
  // The ring is filled BYD first and Marstek after it, each in its own green, so
  // the boundary between the two shows which pack is holding the charge.
  highValue = min((int)floor(status.batteryStoredWh / 1000), 16);
  lowValue = floor((status.batteryStoredWh - highValue * 1000) / 100);
  uint8_t bydLeds = min((uint8_t)round(status.bydStoredWh / 1000), highValue);
  bool batteryFull =
      status.batteryCapacityWh > 0 &&
      status.batteryStoredWh >= 0.99f * status.batteryCapacityWh;
  paintSquare(pixels, 0, 11, bydColors, highValue, lowValue, batteryFull,
              &marstekColors, bydLeds, status.marstekStoredWh >= 100);

  // Network: what we exchange with the grid, magnitude either way — which way it
  // goes is read off the flux entering or leaving the square.
  highValue = floor(status.networkPower / 500);
  lowValue = floor((status.networkPower - highValue * 500) / 50);
  paintSquare(pixels, 11, 0, networkColors, highValue, lowValue);
  // Consumption
  highValue = floor(status.currentLoad / 500);
  lowValue = floor((status.currentLoad - highValue * 500) / 50);
  paintSquare(pixels, 11, 11, loadColors, highValue, lowValue);

  // Flux Network to Consumption
  paintFlux(pixels, 13, 5, 13, 11, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromNetworkToLoad));

  // Flux PV to Network
  paintFlux(pixels, 5, 2, 11, 2, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromPVToNetwork));

  // Flux PV to Battery
  paintFlux(pixels, 2, 5, 2, 11, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromPVToBattery));

  // Flux PV to Consumption
  paintFlux(pixels, 5, 5, 11, 11, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromPVToLoad));

  // Flux Battery to Consumption
  paintFlux(pixels, 5, 13, 11, 13, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
            Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
            getFluxSpeed(counter, status.fromBatteryToLoad));

  // Flux Battery <-> Network, along the free anti-diagonal. The Marstek units
  // can be charged from the grid (and discharged into it), which the Fronius
  // alone could never report, so only one of the two directions is ever active.
  if (status.fromNetworkToBattery > status.fromBatteryToNetwork) {
    paintFlux(pixels, 11, 4, 5, 10, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
              Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
              getFluxSpeed(counter, status.fromNetworkToBattery));
  } else {
    paintFlux(pixels, 5, 10, 11, 4, Adafruit_NeoPixel::Color(0x00, 0x00, 0xff),
              Adafruit_NeoPixel::Color(0x00, 0x00, 0x10),
              getFluxSpeed(counter, status.fromBatteryToNetwork));
  }
}