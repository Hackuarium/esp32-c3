#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "./params.h"
#include "./pixels.h"

#define HAS_WON 1
#define HAS_LOST 2
#define HAS_LOST_PREVIOUSLY 3  // didn't loose during the last update

int32_t snakeCounter = 0;
uint8_t previousNumberPlayers = 0;

uint8_t stateSnake[MAX_LED] = {0};
// 0: not used
// 1: player 1
// 2: player 2
// 3: food

struct player_t {
  uint8_t id;
  position_t head;
  uint8_t length;
  uint32_t color;
  uint8_t directionParameter;
  uint8_t from;
  uint8_t to;
  uint16_t leds[256];  // needs to be 256 because we use from / to overflow in
                       // order not to use modulo
  uint8_t status;      // 0 running, //1: win, //2: lost
};

player_t players[4];

void updatePlayers(Adafruit_NeoPixel& pixels, player_t players[]);
void updateFood(Adafruit_NeoPixel& pixels);
bool isEndGame(player_t players[]);
void resetSnake(Adafruit_NeoPixel& pixels);
void displayResults(Adafruit_NeoPixel& pixels, player_t players[]);
void convertToFood(Adafruit_NeoPixel& pixels, uint8_t id);

void updateSnake(Adafruit_NeoPixel& pixels) {
  snakeCounter++;

  if (getParameter(PARAM_NB_PLAYERS) != previousNumberPlayers) {
    resetSnake(pixels);
    previousNumberPlayers = getParameter(PARAM_NB_PLAYERS);
  }

  if (isEndGame(players) == true || snakeCounter < 0) {
    displayResults(pixels, players);
    if (snakeCounter > 0) {
      snakeCounter = -100;
    }
    if (snakeCounter == 0) {
      resetSnake(pixels);
    }
  } else {
    if (snakeCounter % 10 == 0) {
      updatePlayers(pixels, players);
    }
    if (snakeCounter % 500 == 50) {
      updateFood(pixels);
    }
  }
}

void resetSnake(Adafruit_NeoPixel& pixels) {
  pixels.clear();

  if (getParameter(PARAM_NB_PLAYERS) < 1 ||
      getParameter(PARAM_NB_PLAYERS) > 4) {
    setParameter(PARAM_NB_PLAYERS, 4);
  }

  players[0] = {
      1, {0, 0}, 1, Adafruit_NeoPixel::Color(255, 0, 0), PARAM_COMMAND_1, 0,
      0, {0},    0};
  players[0].leds[0] = getLedIndex(0, 0);
  players[1] = {
      2,
      {getParameter(PARAM_NB_ROWS) - 1, getParameter(PARAM_NB_COLUMNS) - 1},
      1,
      Adafruit_NeoPixel::Color(0, 0, 255),
      PARAM_COMMAND_2,
      0,
      0,
      {0},
      0};
  players[1].leds[0] = getLedIndex(getParameter(PARAM_NB_ROWS) - 1,
                                   getParameter(PARAM_NB_COLUMNS) - 1);
  players[2] = {3,
                {getParameter(PARAM_NB_COLUMNS) - 1, 0},
                1,
                Adafruit_NeoPixel::Color(0, 255, 0),
                PARAM_COMMAND_3,
                0,
                0,
                {0},
                0};
  players[2].leds[0] = getLedIndex(getParameter(PARAM_NB_ROWS) - 1, 0);

  players[3] = {4,
                {0, getParameter(PARAM_NB_COLUMNS) - 1},
                1,
                Adafruit_NeoPixel::Color(255, 0, 255),
                PARAM_COMMAND_4,
                0,
                0,
                {0},
                0};
  players[3].leds[0] = getLedIndex(0, getParameter(PARAM_NB_COLUMNS) - 1);

  snakeCounter = 0;
  setParameter(PARAM_COMMAND_1, 2);
  setParameter(PARAM_COMMAND_2, 1);
  setParameter(PARAM_COMMAND_3, 3);
  setParameter(PARAM_COMMAND_4, 4);
  for (uint16_t i = 0; i < MAX_LED; i++) {
    stateSnake[i] = 0;
  }
}

void updateFood(Adafruit_NeoPixel& pixels) {
  for (uint16_t i = 0; i < MAX_LED; i++) {
    if (stateSnake[i] == 99) {
      stateSnake[i] = 0;
      pixels.setPixelColor(i, Adafruit_NeoPixel::Color(0, 0, 0));
    }
  }
  for (uint16_t i = 0; i < 40; i++) {
    uint16_t led = random(0, MAX_LED);
    if (stateSnake[led] == 0) {
      pixels.setPixelColor(led, Adafruit_NeoPixel::Color(255, 255, 0));
      stateSnake[led] = 99;
    }
  }
}

void updatePlayers(Adafruit_NeoPixel& pixels, player_t players[]) {
  for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
    if (players[i].status == HAS_LOST) {
      players[i].status = HAS_LOST_PREVIOUSLY;
    }
  }

  for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
    if (players[i].status)
      continue;
    switch (getParameter(players[i].directionParameter)) {
      case 1:  // left
        if (players[i].head.column > 0)
          players[i].head.column--;
        else
          players[i].head.column = getParameter(PARAM_NB_COLUMNS) - 1;
        break;
      case 2:  // right
        if (players[i].head.column < (getParameter(PARAM_NB_COLUMNS) - 1))
          players[i].head.column++;
        else
          players[i].head.column = 0;
        break;
      case 3:  // top
        if (players[i].head.row > 0)
          players[i].head.row--;
        else
          players[i].head.row = getParameter(PARAM_NB_ROWS) - 1;
        break;
      case 4:  // bottom
        if (players[i].head.row < (getParameter(PARAM_NB_ROWS) - 1))
          players[i].head.row++;
        else
          players[i].head.row = 0;
        break;
    }
    uint16_t newHead = getLedIndex(players[i].head);
    if (newHead == players[i].leds[players[i].to]) {  // why is this required ?
      continue;
    }
    players[i].leds[++players[i].to] = newHead;
    if (stateSnake[newHead] == 99) {
      // food !
      stateSnake[newHead] = players[i].id;
      pixels.setPixelColor(newHead, players[i].color);
      if ((players[i].to - players[i].from + 1) >=
          getParameter(PARAM_WIN_LIMIT)) {
        players[i].status = HAS_WON;
      }
    } else if (stateSnake[newHead] == 0) {
      stateSnake[newHead] = players[i].id;
      stateSnake[players[i].leds[players[i].from]] = 0;
      pixels.setPixelColor(players[i].leds[players[i].from++],
                           Adafruit_NeoPixel::Color(0, 0, 0));
      pixels.setPixelColor(newHead, players[i].color);
    } else {
      // collision !!
      players[i].status = HAS_LOST;
    }
  }

  // check if players share the same head in this case both loose
  for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
    for (uint8_t j = i + 1; j < getParameter(PARAM_NB_PLAYERS); j++) {
      if ((players[i].status == 0 || players[i].status == HAS_LOST) &&
          (players[j].status == 0 || players[j].status == HAS_LOST)) {
        if (players[i].leds[players[i].to] == players[j].leds[players[j].to]) {
          players[i].status = HAS_LOST;
          players[j].status = HAS_LOST;
        }
      }
    }
  }

  for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
    if (players[i].status == HAS_LOST) {
      convertToFood(pixels, players[i].id);
    }
  }
}

void convertToFood(Adafruit_NeoPixel& pixels, uint8_t id) {
  for (uint16_t i = 0; i < MAX_LED; i++) {
    if (stateSnake[i] == id) {
      stateSnake[i] = 99;
      pixels.setPixelColor(i, Adafruit_NeoPixel::Color(255, 255, 0));
    }
  }
}

bool isEndGame(player_t players[]) {
  // it is the end of the game if somebody won or everybody lost
  uint8_t numberHasLost = 0;
  for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
    if (players[i].status == HAS_WON) {
      return true;
    }
    if (players[i].status &
        HAS_LOST) {  // bitwise operation, lost now or previously
      numberHasLost++;
    }
  }
  if ((numberHasLost > (getParameter(PARAM_NB_PLAYERS) - 2) &&
       getParameter(PARAM_NB_PLAYERS) > 1) ||
      (numberHasLost == getParameter(PARAM_NB_PLAYERS))) {
    return true;
  }
  return false;
}

void displayResults(Adafruit_NeoPixel& pixels, player_t players[]) {
  bool winners = false;
  uint8_t nbLost = 0;

  for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
    if (players[i].status == HAS_WON) {
      winners = true;
    }
    if (players[i].status == HAS_LOST) {
      nbLost++;
    }
  }
  if (!winners && getParameter(PARAM_NB_PLAYERS) > 1) {  // last survivor ?
    for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
      if (players[i].status == 0) {
        players[i].status = HAS_WON;
        winners = true;
      }
    }
  }
  if (!winners) {
    for (uint8_t i = 0; i < getParameter(PARAM_NB_PLAYERS); i++) {
      if (players[i].status == HAS_LOST) {
        players[i].status = HAS_WON;
        winners = true;
      }
    }
  }
  uint16_t counter = 0;
  for (uint16_t i = 0; i < MAX_LED; i++) {
    while (true) {
      uint8_t playerIndex = counter++ % getParameter(PARAM_NB_PLAYERS);
      if (players[playerIndex].status == HAS_WON) {
        pixels.setPixelColor(i, players[playerIndex].color);
        break;
      }
    }
  }
}