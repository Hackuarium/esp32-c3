#include <Adafruit_NeoPixel.h>
#include "./common.h"

void paintSymbol(Adafruit_NeoPixel& pixels,
                 uint8_t ascii,
                 uint8_t x,
                 uint8_t y);
void paintSymbol(Adafruit_NeoPixel& pixels,
                 uint8_t ascii,
                 uint8_t x,
                 uint8_t y,
                 uint32_t ColorFromPalette);