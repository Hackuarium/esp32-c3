#include "droneIdFrames.h"

#include <string.h>

/* No config.h and no THR_DRONE_ID guard, unlike every other file here: that
   include drags in Arduino.h, and being compilable without it is the point -
   these three functions are what a host test can run against frames built by
   the reference transmitter. On a board that is not the drone tracker nothing
   calls them and the linker drops them. */

/* Bluetooth: AD type "Service Data - 16-bit UUID", then the UUID 0xFFFA that
   the Bluetooth SIG assigned to ASTM International, least significant byte
   first as every 16-bit UUID is written on the air, then the application code
   ASTM assigned to Open Drone ID within it. Four byte comparisons reject
   everything else before anything is copied. */
#define BLUETOOTH_AD_SERVICE_DATA_16 0x16
#define ODID_UUID_LOW 0xFA
#define ODID_UUID_HIGH 0xFF
#define ODID_APPLICATION_CODE 0x0D

/* Wi-Fi: the identifier ASD-STAN registered, in this order and not byte
   swapped, then the same 0x0D. */
static const uint8_t astmOui[3] = {0xFA, 0x0B, 0xBC};
#define WIFI_ELEMENT_VENDOR_SPECIFIC 0xDD
#define WIFI_ODID_VENDOR_TYPE 0x0D
/* the 24-byte management header, then timestamp, beacon interval, capability */
#define WIFI_BEACON_ELEMENTS_OFFSET 36

/* A service discovery frame is addressed to the NAN network address rather
   than to a station, so one six-byte comparison rejects every other action
   frame on the air. */
static const uint8_t nanNetworkAddress[6] = {0x51, 0x6F, 0x9A,
                                             0x01, 0x00, 0x00};
static const uint8_t wifiAllianceOui[3] = {0x50, 0x6F, 0x9A};
/* the first six bytes of the SHA-256 of "org.opendroneid.remoteid" */
static const uint8_t odidServiceId[6] = {0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};

const uint8_t* droneIdFindBluetoothPayload(const uint8_t* advertisement,
                                           size_t length,
                                           uint8_t* payloadLength) {
  size_t at = 0;
  while (at + 1 < length) {
    uint8_t fieldLength = advertisement[at];
    /* a zero length is the padding some stacks leave after the last structure;
       it ends the walk rather than being an error */
    if (fieldLength == 0 || at + 1 + fieldLength > length) {
      return NULL;
    }
    const uint8_t* field = &advertisement[at + 1];
    if (fieldLength >= 5 && field[0] == BLUETOOTH_AD_SERVICE_DATA_16 &&
        field[1] == ODID_UUID_LOW && field[2] == ODID_UUID_HIGH &&
        field[3] == ODID_APPLICATION_CODE) {
      /* the length counts the type byte, the two UUID bytes and the
         application code, none of which the decoder wants */
      uint8_t bodyLength = (uint8_t)(fieldLength - 4);
      if (bodyLength > DRONE_MAX_PAYLOAD) {
        return NULL;
      }
      *payloadLength = bodyLength;
      return &field[4];
    }
    at += 1 + fieldLength;
  }
  return NULL;
}

const uint8_t* droneIdFindBeaconPayload(const uint8_t* frame, uint16_t length,
                                        uint8_t* payloadLength) {
  uint16_t at = WIFI_BEACON_ELEMENTS_OFFSET;
  while (at + 2 <= length) {
    uint8_t elementId = frame[at];
    uint8_t elementLength = frame[at + 1];
    /* a length that runs off the end makes every element after it unfindable,
       so the walk stops rather than guessing where the next one starts */
    if ((uint32_t)at + 2 + elementLength > length) {
      return NULL;
    }
    if (elementId == WIFI_ELEMENT_VENDOR_SPECIFIC && elementLength >= 5 &&
        memcmp(&frame[at + 2], astmOui, sizeof(astmOui)) == 0 &&
        frame[at + 5] == WIFI_ODID_VENDOR_TYPE) {
      /* the element holds the identifier and the vendor type, then the counter
         and the messages - the same shape the Bluetooth service data has */
      uint8_t bodyLength = (uint8_t)(elementLength - 4);
      if (bodyLength > DRONE_MAX_PAYLOAD) {
        return NULL;
      }
      *payloadLength = bodyLength;
      return &frame[at + 6];
    }
    at = (uint16_t)(at + 2 + elementLength);
  }
  return NULL;
}

const uint8_t* droneIdFindNanPayload(const uint8_t* frame, uint16_t length,
                                     uint8_t* payloadLength) {
  /* through the service descriptor attribute and the length byte that says how
     much follows it */
  if (length < 43) {
    return NULL;
  }
  if (memcmp(&frame[4], nanNetworkAddress, sizeof(nanNetworkAddress)) != 0) {
    return NULL;
  }
  /* public action frame, vendor specific, Wi-Fi Alliance, NAN */
  if (frame[24] != 0x04 || frame[25] != 0x09 ||
      memcmp(&frame[26], wifiAllianceOui, sizeof(wifiAllianceOui)) != 0 ||
      frame[29] != 0x13) {
    return NULL;
  }
  /* the service descriptor attribute, for the Open Drone ID service, sent
     unsolicited as a follow-up */
  if (frame[30] != 0x03 ||
      memcmp(&frame[33], odidServiceId, sizeof(odidServiceId)) != 0 ||
      frame[39] != 0x01 || frame[41] != 0x10) {
    return NULL;
  }
  uint8_t serviceInfoLength = frame[42];
  if (serviceInfoLength > DRONE_MAX_PAYLOAD ||
      (uint32_t)43 + serviceInfoLength > length) {
    return NULL;
  }
  *payloadLength = serviceInfoLength;
  return &frame[43];
}

ODID_messagetype_t droneIdDecodePayload(const uint8_t* payload, uint8_t length,
                                        ODID_UAS_Data* record) {
  /* The counter is the transport's, not the message's. It changes when the
     content changes and is allowed to stay put when it does not, so it can
     neither count losses nor order frames, and it is skipped rather than
     kept. */
  if (length < 1 + ODID_MESSAGE_SIZE) {
    return ODID_MESSAGETYPE_INVALID;
  }
  const uint8_t* body = payload + 1;
  uint8_t bodyLength = (uint8_t)(length - 1);

  if (decodeMessageType(body[0]) == ODID_MESSAGETYPE_PACKED) {
    /* A pack whose claimed size is larger than what arrived is refused whole:
       half a pack decodes into a position belonging to no message. */
    uint8_t count = body[2];
    if (bodyLength < 3 ||
        (uint16_t)bodyLength < 3 + (uint16_t)count * ODID_MESSAGE_SIZE) {
      return ODID_MESSAGETYPE_INVALID;
    }
  }
  return decodeOpenDroneID(record, body);
}
