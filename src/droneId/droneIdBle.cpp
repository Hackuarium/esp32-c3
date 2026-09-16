#include "config.h"
#ifdef THR_DRONE_ID
#include <NimBLEDevice.h>
#include <string.h>

#include "droneIdBle.h"
#include "droneIdFrames.h"
#include "droneIdQueue.h"
#include "droneIdSurvey.h"

/* Advertisements are cheap and a drone repeats its position once a second, so
   the scan runs at full duty: window equal to interval. Anything less and the
   controller is off channel, or on the other PHY, when the frame arrives. */
#ifndef DRONE_BLE_SCAN_INTERVAL_MS
#define DRONE_BLE_SCAN_INTERVAL_MS 100
#endif

static NimBLEScan* bleScan = NULL;
static uint32_t truncatedAdvertisements = 0;
/* Every advertisement heard, Remote ID or not. It is what separates an
   empty sky from a scanner that is not running: in any inhabited place
   this climbs by hundreds a minute, and a zero here means the radio, not
   the airspace. */
static uint32_t advertisementsHeard = 0;

/* The first fragment of an extended advertisement the controller had to split.

   Past 229 bytes a report is cut in two and NimBLE 1.4.3 does not reassemble
   them, so a nine-message pack - 234 bytes on the air - arrives as a head that
   claims a length longer than the report and a five-byte tail that is not an
   AD structure at all. Neither reaches the decoder, so neither is counted
   there; this is what says out loud that a transmitter is being lost rather
   than merely absent. Packs of eight messages and fewer, which is what
   transmitters actually send, fit in one report and never come here. */
static boolean isTruncatedFragment(const uint8_t* payload, size_t length) {
  return length >= 5 && payload[0] > 0 && (size_t)1 + payload[0] > length &&
         payload[1] == 0x16 && payload[2] == 0xFA && payload[3] == 0xFF &&
         payload[4] == 0x0D;
}

class DroneIdScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertised) override {
    /* Runs on the NimBLE host task, whose stack is 4 kB: filter, copy, return.
       Nothing here decodes, allocates or prints - the drone task does that. */
    advertisementsHeard++;
    size_t length = advertised->getPayloadLength();
    if (length == 0) {
      return;
    }
    DroneCapture capture;
    uint8_t bodyLength = 0;
    const uint8_t* advertisement = advertised->getPayload();
    droneIdSurveyAdvertisement(advertisement, length);
    const uint8_t* body =
        droneIdFindBluetoothPayload(advertisement, length, &bodyLength);
    if (body == NULL) {
      if (isTruncatedFragment(advertisement, length)) {
        truncatedAdvertisements++;
      }
      return;
    }

    /* getNative() is the address least significant byte first, which is how
       the radio carries it and the reverse of how anybody writes one down.

       The address is held in a named local first, and that is not tidiness:
       getAddress() returns by value and getNative() points into the copy, so
       reading through it after the semicolon would be reading a destroyed
       object. The bytes happen to survive on this build, which is exactly what
       makes it worth writing down - these six are what the table matches on,
       and nothing would say when a change of compiler stopped it working. */
    NimBLEAddress address = advertised->getAddress();
    const uint8_t* native = address.getNative();
    for (uint8_t i = 0; i < DRONE_ADDRESS_LENGTH; i++) {
      capture.address[i] = native[DRONE_ADDRESS_LENGTH - 1 - i];
    }
#if CONFIG_BT_NIMBLE_EXT_ADV
    capture.source = advertised->isLegacyAdvertisement()
                         ? DRONE_SOURCE_BLE_LEGACY
                         : DRONE_SOURCE_BLE_EXTENDED;
#else
    capture.source = DRONE_SOURCE_BLE_LEGACY;
#endif
    capture.rssi = (int8_t)advertised->getRSSI();
    capture.channel = 0;
    capture.length = bodyLength;
    memcpy(capture.payload, body, bodyLength);
    droneIdQueuePush(&capture);
  }
};

static DroneIdScanCallbacks scanCallbacks;

void droneIdBleBegin() {
  NimBLEDevice::init("");
  bleScan = NimBLEDevice::getScan();
  /* Passive. Remote ID advertises non-connectable and non-scannable, so there
     is no scan response to ask for, and a scan request would make a receiver
     that exists to listen transmit several times a second. */
  bleScan->setActiveScan(false);
  bleScan->setInterval(DRONE_BLE_SCAN_INTERVAL_MS);
  bleScan->setWindow(DRONE_BLE_SCAN_INTERVAL_MS);
  /* 0 keeps no results: the device is deleted right after the callback, which
     is what stops a vector growing for as long as the scan runs. */
  bleScan->setMaxResults(0);
  /* The second argument is what turns the controller's duplicate filter OFF,
     and it is load bearing: an aircraft sends its identity, its position and
     its operator's position from one address, and a filter that reports each
     address once would deliver the first of those and nothing else, ever. */
  bleScan->setAdvertisedDeviceCallbacks(&scanCallbacks, true);
}

void droneIdBleListen(boolean listening) {
  if (bleScan == NULL) {
    return;
  }
  if (!listening) {
    if (bleScan->isScanning()) {
      bleScan->stop();
    }
    return;
  }
  /* Restarted whenever it is found stopped, which happens for two reasons and
     both are ordinary: the window before this one stopped it, and a scan
     started with duration 0 under extended advertising is not endless. NimBLE
     turns 0 into BLE_HS_FOREVER and then divides by 10 for the extended API's
     10 ms units, where the parameter is 16 bits - so it truncates to about
     524 seconds and the controller stops. A receiver that started once and
     never looked again would go deaf after nine minutes. */
  if (!bleScan->isScanning()) {
    bleScan->start(0, NULL, false);
  }
}

boolean droneIdBleScanning() {
  return bleScan != NULL && bleScan->isScanning();
}

uint32_t droneIdBleAdvertisements() { return advertisementsHeard; }

uint32_t droneIdBleTruncated() { return truncatedAdvertisements; }
#endif
