#include "FastLED.h"
#include "common.h"
#include "pixels.h"

const uint16_t font53[96] = {
    0b0000000000000000,  // 32
    0b0000000000000000,  // 33
    0b0000000000000000,  // 34
    0b0000000000000000,  // 35
    0b0000000000000000,  // 36
    0b1000010101000010,  // 37 %
    0b0000000000000000,  // 38
    0b0000000000000000,  // 39
    0b0000000000000000,  // 40
    0b0000000000000000,  // 41
    0b0000000000000000,  // 42
    0b0000101110100000,  // 43 +
    0b0000000000000000,  // 44
    0b0000001110000000,  // 45 -
    0b0000000000000000,  // 46
    0b0000000000000000,  // 47
    0b0101011011010100,  // 48 0
    0b0100100100100100,  // 49 1
    0b1100010101001110,  // 50
    0b1110010110011110,  // 51
    0b1011011110010010,  // 52
    0b1111001110011100,  // 53
    0b0111001111011110,  // 54
    0b1110010101001000,  // 55
    0b1111011111011110,  // 56
    0b1111011110011100,  // 57
    0b0000000000000000,  // 58
    0b0000000000000000,  // 59
    0b0000000000000000,  // 60
    0b0000000000000000,  // 61
    0b0000000000000000,  // 62
    0b0000000000000000,  // 63
    0b0000000000000000,  // 64
    0b0000000000000000,  // 65
    0b0000000000000000,  // 66
    0b0000000000000000,  // 67
    0b0000000000000000,  // 68
    0b0000000000000000,  // 69
    0b0000000000000000,  // 70
    0b0000000000000000,  // 71
    0b0000000000000000,  // 72
    0b0000000000000000,  // 73
    0b0000000000000000,  // 74
    0b0000000000000000,  // 75
    0b0000000000000000,  // 76
    0b0000000000000000,  // 77
    0b0000000000000000,  // 78
    0b0000000000000000,  // 79
    0b0000000000000000,  // 80
    0b0000000000000000,  // 81
    0b0000000000000000,  // 82
    0b0000000000000000,  // 83
    0b0000000000000000,  // 84
    0b0000000000000000,  // 85
    0b0000000000000000,  // 86
    0b0000000000000000,  // 87
    0b0000000000000000,  // 88
    0b0000000000000000,  // 89
    0b0000000000000000,  // 90
    0b0000000000000000,  // 91
    0b0000000000000000,  // 92
    0b0000000000000000,  // 93
    0b0000000000000000,  // 94
    0b0000000000000000   // 95
};

void paintSymbol(CRGB pixels[],
                 uint8_t ascii,
                 uint8_t x,
                 uint8_t y,
                 CRGB color) {
  uint16_t symbol = font53[ascii - 32];
  for (uint8_t bit = 0; bit < 15; bit++) {
    int8_t column = x + bit % 3;
    int8_t row = y + bit / 3;
    uint16_t led = getLedIndex(row, column);
    if (bitRead(symbol, 15 - bit)) {
      pixels[led] = color;
    } else {
      pixels[led] = CRGB(0);
    }
  }
}

void paintSymbol(CRGB pixels[], uint8_t ascii, uint8_t x, uint8_t y) {
  paintSymbol(pixels, ascii, x, y, CRGB(255, 0, 255));
}