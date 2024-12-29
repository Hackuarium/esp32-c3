
#include <Adafruit_NeoPixel.h>
#include "../pixels.h"
#include "config.h"
#include "fronius.h"

void paintSquare(Adafruit_NeoPixel& pixels,
                 uint8_t row,
                 uint8_t column,
                 uint32_t highColor,
                 uint32_t lowColor,
                 uint32_t backgroundColor,
                 uint8_t highValue,
                 uint8_t lowValue) {
  if (highValue > 16) {  // we fill completely the square with bright color
    for (uint8_t i = 0; i < 5; ++i) {
      for (uint8_t j = 0; j < 5; ++j) {
        pixels.setPixelColor(getLedIndex(row + i, column + j), highColor);
      }
    }
    return;
  }

  static uint8_t rowLevels[16] = {0, 0, 0, 0, 0, 1, 2, 3,
                                  4, 4, 4, 4, 4, 3, 2, 1};
  static uint8_t columnLevels[16] = {0, 1, 2, 3, 4, 4, 4, 4,
                                     4, 3, 2, 1, 0, 0, 0, 0};

  for (uint8_t i = 0; i < 5; ++i) {
    for (uint8_t j = 0; j < 5; ++j) {
      pixels.setPixelColor(getLedIndex(row + i, column + j), backgroundColor);
    }
  }

  for (uint8_t i = 0; i < highValue; ++i) {
    pixels.setPixelColor(
        getLedIndex(row + rowLevels[i], column + columnLevels[i]), highColor);
    pixels.setPixelColor(
        getLedIndex(row + rowLevels[i], column + columnLevels[i]), highColor);
  }

  // details information in the centrum
  for (uint8_t i = 0; i < min(lowValue, (uint8_t)9); ++i) {
    pixels.setPixelColor(
        getLedIndex(row + 1 + floor(i / 3), column + 1 + i % 3), lowColor);
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

  // PV
  uint8_t highValue = floor(status.powerFromPV / 500);
  uint8_t lowValue = floor((status.powerFromPV - highValue * 500) / 50);
  paintSquare(pixels, 0, 0, Adafruit_NeoPixel::Color(0xff, 0xff, 0x00),
              Adafruit_NeoPixel::Color(0x50, 0x50, 0x00),
              Adafruit_NeoPixel::Color(0x10, 0x10, 0x00), highValue, lowValue);
  // Battery

  paintSquare(pixels, 0, 11, Adafruit_NeoPixel::Color(0x00, 0xff, 0x00),
              Adafruit_NeoPixel::Color(0, 0, 0),
              Adafruit_NeoPixel::Color(0x00, 0x10, 0x00),
              (uint8_t)round((status.batteryChargePercentage - 6) / 5.68), 0);
  // Network
  float networkLevel = status.powerFromGrid;
  if (networkLevel < 0)
    networkLevel = -networkLevel;
  highValue = floor(networkLevel / 500);
  lowValue = floor((networkLevel - highValue * 500) / 50);
  paintSquare(pixels, 11, 0, Adafruit_NeoPixel::Color(0xff, 0xff, 0xff),
              Adafruit_NeoPixel::Color(0x50, 0x50, 0x50),
              Adafruit_NeoPixel::Color(0x10, 0x10, 0x10), highValue, lowValue);
  // Consumption
  highValue = floor(status.currentLoad / 500);
  lowValue = floor((status.currentLoad - highValue * 500) / 50);
  paintSquare(pixels, 11, 11, Adafruit_NeoPixel::Color(0xff, 0x00, 0x00),
              Adafruit_NeoPixel::Color(0x50, 0x00, 0x00),
              Adafruit_NeoPixel::Color(0x10, 0x00, 0x00), highValue, lowValue);

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
}