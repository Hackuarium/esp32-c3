#ifndef _AIR_RADIO_H
#define _AIR_RADIO_H

#include <Arduino.h>

#include "airProtocols.h"

/* The SX1262 of the air traffic receiver: one radio, switched between the
   settings of airProtocols.h as the schedule asks, and never transmitting.

   Captures are read out of the chip by the task, never in the interrupt: the
   interrupt only notes that a packet is waiting and when it arrived. */

/* A LoRa payload can be 255 bytes; nothing else here is longer than 64. */
#define AIR_RAW_MAX 255

typedef struct {
  AirListen listen;
  uint8_t raw[AIR_RAW_MAX];
  uint16_t length;
  int16_t rssi;
  /* millis() when the chip raised the interrupt, the end of the packet */
  uint32_t atMillis;
  /* LoRa only: the header announced a CRC, and the chip checked it */
  boolean loraCrcChecked;
} AirCapture;

/* False when the chip does not answer; the task then says so and stops. */
boolean airRadioBegin();

/* Puts the radio on a setting. Switching between two GFSK settings is a
   frequency and a sync word; switching to or from LoRa re-initialises the chip,
   which is what (ti) reports as the slowest switch. */
boolean airRadioListen(AirListen listen);

AirListen airRadioListening();

/* Reads the packet waiting in the chip, if there is one, and listens again. */
boolean airRadioTake(AirCapture* capture);

/* Packets the chip itself refused: a LoRa CRC that failed. */
uint32_t airRadioRefused();

uint32_t airRadioSlowestSwitchMillis();

#endif
