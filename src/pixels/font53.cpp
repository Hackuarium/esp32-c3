#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
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
    0b1100010101001110,  // 50 2
    0b1110010110011110,  // 51 3
    0b1011011110010010,  // 52 4
    0b1111001110011100,  // 53 5
    0b0111001111011110,  // 54 6
    0b1110010101001000,  // 55 7
    0b1111011111011110,  // 56 8
    0b1111011110011100,  // 57 9
    0b0000000000000000,  // 58
    0b0000000000000000,  // 59
    0b0000000000000000,  // 60
    0b0000000000000000,  // 61
    0b0000000000000000,  // 62
    0b0000000000000000,  // 63
    0b0000000000000000,  // 64
    0b1111011111011010,  // 65 A
    0b0000000000000000,  // 66 B
    0b0000000000000000,  // 67 C
    0b0000000000000000,  // 68 D
    0b0000000000000000,  // 69 E
    0b0000000000000000,  // 70 F
    0b0000000000000000,  // 71 G
    0b0000000000000000,  // 72 H
    0b0000000000000000,  // 73 I
    0b0000000000000000,  // 74 J
    0b0000000000000000,  // 75 K
    0b0000000000000000,  // 76 L
    0b0000000000000000,  // 77 M
    0b0000000000000000,  // 78 N
    0b0000000000000000,  // 79 O
    0b0000000000000000,  // 80 P
    0b0000000000000000,  // 81 Q
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

void paintSymbol(Adafruit_NeoPixel& pixels,
                 uint8_t ascii,
                 uint8_t x,
                 uint8_t y,
                 uint32_t color) {
  uint16_t symbol = font53[ascii - 32];
  for (uint8_t bit = 0; bit < 15; bit++) {
    int8_t column = x + bit % 3;
    int8_t row = y + bit / 3;
    uint16_t led = getLedIndex(row, column);
    if (bitRead(symbol, 15 - bit)) {
      pixels.setPixelColor(led, color);
    } else {
      pixels.setPixelColor(led, 0);
    }
  }
}

void paintSymbol(Adafruit_NeoPixel& pixels,
                 uint8_t ascii,
                 uint8_t x,
                 uint8_t y) {
  paintSymbol(pixels, ascii, x, y, Adafruit_NeoPixel::Color(255, 0, 255));
}

#endif