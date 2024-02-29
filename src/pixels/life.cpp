#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "./pixels.h"
#include "font53.h"
#include "params.h"

struct cellState_t {
  unsigned int state : 4;
  unsigned int neighbours : 4;
};

int32_t lifeCounter = 0;
cellState_t lifeState[MAX_LED] = {0};
uint8_t previousActive = 0;
uint8_t stableActive = 0;
uint16_t generation = 0;

void updateLife(Adafruit_NeoPixel& pixels);
void resetLife(Adafruit_NeoPixel& pixels);
void paintLife(Adafruit_NeoPixel& pixels);

void updateLife(Adafruit_NeoPixel& pixels, uint8_t programChanged) {
  if (programChanged) {
    setParameter(PARAM_COMMAND_1, -1);
    setParameter(PARAM_COMMAND_5, 0);
    setParameter(PARAM_COMMAND_6, 0);
  }

  if (getParameter(PARAM_COMMAND_1) == -1) {
    resetLife(pixels);
    setParameter(PARAM_COMMAND_1, 0);
    lifeCounter = getParameter(PARAM_SPEED) == 20 ? 0 : -200;
  }

  lifeCounter++;

  if (lifeCounter < 0) {
    return;
  }

  if ((lifeCounter % (21 - getParameter(PARAM_SPEED))) != 0) {
    return;
  }

  uint8_t nbRows = getParameter(PARAM_NB_ROWS);
  uint8_t nbColumns = getParameter(PARAM_NB_COLUMNS);
  // calculate numbers of neighbours
  for (uint8_t row = 0; row < nbRows; row++) {
    for (uint16_t column = 0; column < nbColumns; column++) {
      uint16_t led = getLedIndex(row, column);
      lifeState[getLedIndex(row, column)].neighbours =
          lifeState[getLedIndex((row + nbRows - 1) % 16,
                                (column + nbColumns - 1) % 16)]
              .state +
          lifeState[getLedIndex((row + nbRows - 1) % 16, (column) % 16)].state +
          lifeState[getLedIndex((row + nbRows - 1) % 16, (column + 1) % 16)]
              .state +
          lifeState[getLedIndex((row) % 16, (column + nbColumns - 1) % 16)]
              .state +
          lifeState[getLedIndex((row) % 16, (column + 1) % 16)].state +
          lifeState[getLedIndex((row + 1) % 16, (column + nbColumns - 1) % 16)]
              .state +
          lifeState[getLedIndex((row + 1) % 16, (column) % 16)].state +
          lifeState[getLedIndex((row + 1) % 16, (column + 1) % 16)].state;
    }
  }

  // kill or create
  for (uint8_t row = 0; row < nbRows; row++) {
    for (uint16_t column = 0; column < nbColumns; column++) {
      uint16_t led = getLedIndex(row, column);
      if (lifeState[led].state) {
        if (lifeState[led].neighbours < 2) {
          lifeState[led].state = 0;
        } else if (lifeState[led].neighbours > 3) {
          lifeState[led].state = 0;
        } else {
          lifeState[led].state = 1;
        }
      } else {
        if (lifeState[led].neighbours == 3) {
          lifeState[led].state = 1;
        } else {
          lifeState[led].state = 0;
        }
      }
    }
  }

  paintLife(pixels);
}

void paintLife(Adafruit_NeoPixel& pixels) {
  // paint
  uint8_t nbActive = 0;
  for (uint16_t i = 0; i < MAX_LED; i++) {
    if (lifeState[i].state == 1) {
      nbActive++;
      setColor(pixels, i);
    } else {
      pixels.setPixelColor(i, 0);
    }
  }
  if (nbActive == previousActive) {
    stableActive++;
    if (stableActive > 50) {  // display the nb generation
      pixels.clear();
      String generationStr = String(generation - 51);
      for (uint8_t i = 0; i < generationStr.length(); ++i) {
        uint8_t ascii = (uint8_t)generationStr.charAt(i);
        paintSymbol(pixels, ascii, i * 4, 0);
      }
      if (generation > 300 && stableActive == 51) {
        Serial.print("<option value=\"");
        Serial.print(getParameter(PARAM_COMMAND_3));
        Serial.print("\">");
        Serial.print(getParameter(PARAM_COMMAND_3));
        Serial.print(" (");
        Serial.print(generation - 51);
        Serial.print(")");
        Serial.println("</option>");
      }
      String idStr = String((uint16_t)getParameter(PARAM_COMMAND_3));
      for (uint8_t i = 0; i < min(4, (int)idStr.length()); ++i) {
        uint8_t ascii = (uint8_t)idStr.charAt(i);
        paintSymbol(pixels, ascii, i * 4, 6,
                    Adafruit_NeoPixel::Color(0, 255, 255));
      }
      for (uint8_t i = 4; i < idStr.length(); ++i) {
        uint8_t ascii = (uint8_t)idStr.charAt(i);
        paintSymbol(pixels, ascii, i * 4 + 6, 10,
                    Adafruit_NeoPixel::Color(0, 255, 127));
      }
    } else {
      generation++;
    }
    if (stableActive == 100) {
      setParameter(PARAM_COMMAND_4, getParameter(PARAM_COMMAND_3));
      if ((generation - 50) > getParameter(PARAM_COMMAND_5)) {
        setParameter(PARAM_COMMAND_5, generation - 50);
        setParameter(PARAM_COMMAND_6, getParameter(PARAM_COMMAND_3));
      }
      setParameter(PARAM_COMMAND_1, -1);
    }

  } else {
    generation++;
    previousActive = nbActive;
    stableActive = 0;
  }
}

void resetLife(Adafruit_NeoPixel& pixels) {
  if (getParameter(PARAM_COMMAND_2) == 0) {
    setParameter(PARAM_COMMAND_3, millis() % 65536);
  } else {
    setParameter(PARAM_COMMAND_3, getParameter(PARAM_COMMAND_2));
    setParameter(PARAM_COMMAND_2, getParameter(PARAM_COMMAND_2) + 1);
  }
  srand(getParameter(PARAM_COMMAND_3));

  pixels.clear();
  for (uint16_t i = 0; i < MAX_LED; i++) {
    lifeState[i] = {0, 0};
  }
  for (uint16_t i = 0; i < 40; i++) {
    uint16_t led = rand() % getNbLeds();
    lifeState[led].state = 1;
  }
  generation = 0;
  paintLife(pixels);
}

#endif