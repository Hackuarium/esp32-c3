#include <Adafruit_NeoPixel.h>
#include "common.h"
#include "inttypes.h"
typedef struct position_t {
  uint8_t row;
  uint8_t column;
} position_t;

uint32_t getColor();
uint32_t getColor(uint8_t hsbChangeSpeed);
uint32_t getColor(uint8_t colorModel, uint8_t hsbChangeSpeed);
uint32_t getColor(uint8_t colorModel,
                  uint8_t hsbChangeSpeed,
                  uint8_t intensity);

uint32_t getBackgroundColor();

bool decreaseColor(Adafruit_NeoPixel& pixels, uint16_t led, uint8_t increment);
bool decreaseColor(Adafruit_NeoPixel& pixels, uint16_t led);
void setColor(Adafruit_NeoPixel& pixels, uint16_t led);
uint32_t getHSV360(uint16_t h, uint8_t s, uint8_t v);

int16_t getNextLedIndex(uint8_t row, uint8_t column, uint8_t direction);
uint8_t getDirection();
uint16_t getLedIndex(uint16_t led);

uint16_t getLedIndex(uint8_t row, uint8_t column);
uint16_t getLedIndex(position_t& position);

void updateMapping();

uint8_t getHSVSpeedChange();

void copyPixelColor(Adafruit_NeoPixel& pixels, uint16_t from, uint16_t to);