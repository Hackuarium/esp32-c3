#include "FastLED.h"
#include "common.h"
#include "inttypes.h"
typedef struct position_t {
  uint8_t row;
  uint8_t column;
} position_t;

CRGB getColor();
CRGB getColor(uint8_t hsbChangeSpeed);
CRGB getColor(uint8_t colorModel, uint8_t hsbChangeSpeed);
CRGB getColor(uint8_t colorModel, uint8_t hsbChangeSpeed, uint8_t intensity);

CRGB getBackgroundColor();

bool decreaseColor(CRGB pixels[], uint16_t led, uint8_t increment);
bool decreaseColor(CRGB pixels[], uint16_t led);
void setColor(CRGB pixels[], uint16_t led);
CRGB getHSV360(uint16_t h, uint8_t s, uint8_t v);

int16_t getNextLedIndex(uint8_t row, uint8_t column, uint8_t direction);
uint8_t getDirection();
uint16_t getLedIndex(uint16_t led);

uint16_t getLedIndex(uint8_t row, uint8_t column);
uint16_t getLedIndex(position_t& position);

void updateMapping();
void blank(CRGB pixels[]);

uint8_t getHSVSpeedChange();
