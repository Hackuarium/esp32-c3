#ifndef _LORA_PINS_H
#define _LORA_PINS_H

/* The Wio-SX1262 on a Seeed XIAO ESP32S3. Shared by every board kind that
   drives that module - the mesh and the air traffic receiver - because a board
   builds exactly one of them, and two copies of these numbers is how one of
   them ends up on the wrong pin. Each can be overridden from the env. */

/* SX1262 pins of the Seeed XIAO ESP32S3 LoRa module: CS, DIO1, RESET, BUSY */
#ifndef LORA_PIN_CS
#define LORA_PIN_CS 41
#define LORA_PIN_DIO1 39
#define LORA_PIN_RESET 42
#define LORA_PIN_BUSY 40
#endif
/* The antenna hangs off a PE4259, and that switch takes two control lines:
   DIO2 drives CTRL and this pin drives /CTRL, so they have to move together and
   opposite. Leaving it floating does not fail, it attenuates - the receive path
   still happens to be selected, so a node hears normally and its own transmit
   only reaches the antenna through the switch's isolation, about 40 dB down.
   That is invisible on a bench at arm's length and costs a factor of a hundred
   in range outdoors. */
#ifndef LORA_PIN_RF_SW
#define LORA_PIN_RF_SW 38
#endif

/* The Wio-SX1262 clocks the radio from an active TCXO supplied by DIO3, and it
   is a 1.8 V part. RadioLib's begin() defaults this argument to 1.6 V - the
   lowest step the SX1262 regulator offers - so leaving it out runs the
   oscillator below spec: it still starts, so begin() reports no error, but the
   PLL locks to a marginal reference and the transmitted chirp is spectrally
   smeared. The receiver's correlator then throws most of the energy away, which
   reads as a weak *and* noisy packet - a fixed ~50 dB below budget with the SNR
   pinned near 5 dB however close the nodes are. It has to be stated per board,
   not guessed: a module with a plain crystal needs 0 here instead. */
#ifndef LORA_TCXO_VOLTAGE
#define LORA_TCXO_VOLTAGE 1.8f
#endif

#endif
