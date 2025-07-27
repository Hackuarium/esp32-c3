#ifndef _RADIOLIB_EX_LORAWAN_CONFIG_H
#define _RADIOLIB_EX_LORAWAN_CONFIG_H

#include <RadioLib.h>

// https://github.com/radiolib-org/RadioBoards/blob/cc884a5428fca7f7b04f11669c7d78437e246650/src/maintained/SeeedStudio/XIAO_ESP32S3.h#L10
SX1262 radio = new Module(41, 39, 42, 40);  // CS, DIO1, RESET, BUSY
#define Radio

// if you have RadioBoards (https://github.com/radiolib-org/RadioBoards)
// and are using one of the supported boards, you can do the following:
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>

Radio radio = new RadioModule();
*/

// regional choices: EU868, US915, AU915, AS923, AS923_2, AS923_3, AS923_4,
// IN865, KR920, CN470
const LoRaWANBand_t Region = EU868;

// subband choice: for US915/AU915 set to 2, for CN470 set to 1, otherwise leave
// on 0
const uint8_t subBand = 0;

// ============================================================================
// Below is to support the sketch - only make changes if the notes say so ...

// copy over the keys in to the something that will not compile if incorrectly
// formatted
uint32_t devAddr = 0x00000000;
uint8_t appSKey[16] = {};
uint8_t nwkSEncKey[16] = {0};

uint8_t fNwkSIntKey[16] = {0};
uint8_t sNwkSIntKey[16] = {0};

// create the LoRaWAN node
LoRaWANNode node(&radio, &Region, subBand);

// result code to text - these are error codes that can be raised when using
// LoRaWAN however, RadioLib has many more - see
// https://jgromes.github.io/RadioLib/group__status__codes.html for a complete
// list
String stateDecode(const int16_t result) {
  switch (result) {
    case RADIOLIB_ERR_NONE:
      return "ERR_NONE";
    case RADIOLIB_ERR_CHIP_NOT_FOUND:
      return "ERR_CHIP_NOT_FOUND";
    case RADIOLIB_ERR_PACKET_TOO_LONG:
      return "ERR_PACKET_TOO_LONG";
    case RADIOLIB_ERR_RX_TIMEOUT:
      return "ERR_RX_TIMEOUT";
    case RADIOLIB_ERR_CRC_MISMATCH:
      return "ERR_CRC_MISMATCH";
    case RADIOLIB_ERR_INVALID_BANDWIDTH:
      return "ERR_INVALID_BANDWIDTH";
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR:
      return "ERR_INVALID_SPREADING_FACTOR";
    case RADIOLIB_ERR_INVALID_CODING_RATE:
      return "ERR_INVALID_CODING_RATE";
    case RADIOLIB_ERR_INVALID_FREQUENCY:
      return "ERR_INVALID_FREQUENCY";
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER:
      return "ERR_INVALID_OUTPUT_POWER";
    case RADIOLIB_ERR_NETWORK_NOT_JOINED:
      return "RADIOLIB_ERR_NETWORK_NOT_JOINED";
    case RADIOLIB_ERR_DOWNLINK_MALFORMED:
      return "RADIOLIB_ERR_DOWNLINK_MALFORMED";
    case RADIOLIB_ERR_INVALID_REVISION:
      return "RADIOLIB_ERR_INVALID_REVISION";
    case RADIOLIB_ERR_INVALID_PORT:
      return "RADIOLIB_ERR_INVALID_PORT";
    case RADIOLIB_ERR_NO_RX_WINDOW:
      return "RADIOLIB_ERR_NO_RX_WINDOW";
    case RADIOLIB_ERR_INVALID_CID:
      return "RADIOLIB_ERR_INVALID_CID";
    case RADIOLIB_ERR_UPLINK_UNAVAILABLE:
      return "RADIOLIB_ERR_UPLINK_UNAVAILABLE";
    case RADIOLIB_ERR_COMMAND_QUEUE_FULL:
      return "RADIOLIB_ERR_COMMAND_QUEUE_FULL";
    case RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND:
      return "RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND";
    case RADIOLIB_ERR_JOIN_NONCE_INVALID:
      return "RADIOLIB_ERR_JOIN_NONCE_INVALID";
    case RADIOLIB_ERR_N_FCNT_DOWN_INVALID:
      return "RADIOLIB_ERR_N_FCNT_DOWN_INVALID";
    case RADIOLIB_ERR_A_FCNT_DOWN_INVALID:
      return "RADIOLIB_ERR_A_FCNT_DOWN_INVALID";
    case RADIOLIB_ERR_DWELL_TIME_EXCEEDED:
      return "RADIOLIB_ERR_DWELL_TIME_EXCEEDED";
    case RADIOLIB_ERR_CHECKSUM_MISMATCH:
      return "RADIOLIB_ERR_CHECKSUM_MISMATCH";
    case RADIOLIB_ERR_NO_JOIN_ACCEPT:
      return "RADIOLIB_ERR_NO_JOIN_ACCEPT";
    case RADIOLIB_LORAWAN_SESSION_RESTORED:
      return "RADIOLIB_LORAWAN_SESSION_RESTORED";
    case RADIOLIB_LORAWAN_NEW_SESSION:
      return "RADIOLIB_LORAWAN_NEW_SESSION";
    case RADIOLIB_ERR_NONCES_DISCARDED:
      return "RADIOLIB_ERR_NONCES_DISCARDED";
    case RADIOLIB_ERR_SESSION_DISCARDED:
      return "RADIOLIB_ERR_SESSION_DISCARDED";
  }
  return "See https://jgromes.github.io/RadioLib/group__status__codes.html";
}

// helper function to display any issues
void debug(bool failed,
           const __FlashStringHelper* message,
           int state,
           bool halt) {
  if (failed) {
    Serial.print(message);
    Serial.print(" - ");
    Serial.print(stateDecode(state));
    Serial.print(" (");
    Serial.print(state);
    Serial.println(")");
    while (halt) {
      delay(1);
    }
  }
}

#endif