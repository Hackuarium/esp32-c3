#include "config.h"
#ifdef THR_BLE
#include <NimBLEDevice.h>
#include <string.h>

#include "getMedianInt11.h"
#include "params.h"
#ifdef THR_LORA_MESH
#include "lora/loraBridge.h"
#endif

/* The observer: it never connects to anything, it only listens to the
   advertisements every BLE device shouts into the air, and turns one of them
   into a number the mesh can carry.

   Two jobs, and the scan callback does both. It keeps a table of what is
   around, so (bl) can list it and the operator can pick a device without
   knowing its address in advance; and when an advertisement comes from the
   device (bs) selected, its RSSI goes into PARAM_BLE_RSSI, which the periodic
   broadcast already carries because that slot is adjacent to the GPS fix.

   The selection is an address in NVS rather than a parameter: six bytes do not
   fit in an int16, and a table index would mean something different after
   every reboot. */

/* A bridge reports everything it hears, so the table has to hold a whole
   sweep's worth of a busy place rather than just the handful being watched.

   256 because extended advertising changed the scale of "a busy place": at 48
   a single office produced 275 distinct addresses in six sweeps, meaning every
   slot turned over every time and a device could be heard without ever
   surviving to be reported. A table that evicts faster than it reports does
   not lose the weakest signal, it loses an arbitrary one - which for somebody
   hunting a specific tag is indistinguishable from the tag not being there. */
#define BLE_MAX_DEVICES 255
#define BLE_ADDRESS_LENGTH 18  // "aa:bb:cc:dd:ee:ff"
#define BLE_NAME_LENGTH 21
#define BLE_SELECTION_KEY "ble.mac"

/* Advertisements are cheap, so the scan runs at full duty: window equal to
   interval. A beacon that advertises every two seconds is otherwise missed
   about as often as it is heard. */
#ifndef BLE_SCAN_INTERVAL_MS
#define BLE_SCAN_INTERVAL_MS 100
#endif

/* How long the selected device may stay silent before its RSSI is reported as
   unset rather than as the last value heard. A stale reading is worse than no
   reading here: the point of the measurement is proximity, so "I cannot hear
   it" is the answer, not "-71 dBm, some minutes ago". */
#ifndef BLE_BEACON_TIMEOUT_MS
#define BLE_BEACON_TIMEOUT_MS 30000
#endif

typedef struct {
  char address[BLE_ADDRESS_LENGTH];
  char name[BLE_NAME_LENGTH];
  int16_t rssi;
  uint32_t lastSeenMillis;
  /* both reset at every sweep, so a feed line describes the window it covers
     rather than everything since boot. The strongest sample is the one to
     identify a tag by: it is the least obstructed path, which is what a tag
     held against the bridge produces. */
  uint16_t advertisements;
  int16_t bestRssi;
  /* the TX Power AD field, when the device publishes one at all. It is not the
     calibration - that is the RSSI at a metre, which includes the antenna and
     the body of whatever the tag is glued to - but it is the one hint an
     unknown tag gives about its own model. */
  int8_t txPower;
  boolean haveTxPower;
  /* NimBLE's address type: 0 public, 1 random, 2 and 3 resolvable identities.
     It travels because it is what says whether the address means anything at
     all - only a public one carries an IEEE OUI, and so a manufacturer. Most
     phones and watches rotate a random address every few minutes, and looking
     a vendor up from one of those invents a fact. */
  uint8_t addressType;
#if CONFIG_BT_NIMBLE_EXT_ADV
  /* Whether it was heard as a legacy advertisement or a BLE 5 extended one, and
     which PHY carried it. Reported because "we cannot see it" is otherwise
     unanswerable: a device missing from a legacy-only scan and a device that is
     not advertising at all look exactly alike from here. */
  boolean legacy;
  uint8_t primaryPhy;
#endif
} BleDevice;

#define BLE_ADDRESS_TYPE_PUBLIC 0

static BleDevice devices[BLE_MAX_DEVICES];
static uint8_t deviceCount = 0;

static char selectedAddress[BLE_ADDRESS_LENGTH] = "";
static uint32_t selectedLastSeenMillis = 0;
static boolean selectedEverSeen = false;

/* RSSI swings by ten dB between two advertisements of a motionless beacon, so
   the parameter carries the median of the last eleven rather than the last
   one. The ring is primed with the first sample seen, which makes the median
   defined from the very first advertisement instead of after eleven.

   Eleven because that is what getMedianInt11 reads: it takes no length. */
#define BLE_RSSI_SAMPLES 11
static int16_t rssiSamples[BLE_RSSI_SAMPLES];
static uint8_t rssiSampleIndex = 0;

static SemaphoreHandle_t bleMutex = NULL;
static NimBLEScan* bleScan = NULL;

static boolean bleTake() {
  return bleMutex != NULL &&
         xSemaphoreTake(bleMutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

static void bleGive() {
  xSemaphoreGive(bleMutex);
}

static void forgetBeaconSignal() {
  selectedEverSeen = false;
  selectedLastSeenMillis = 0;
  setParameter(PARAM_BLE_RSSI, ERROR_VALUE);
}

/* Never setAndSaveParameter: an advertisement arrives several times a second
   and NVS is good for about a hundred thousand writes. The value lives in the
   parameter array, which is what the broadcast reads. */
static void recordBeaconSignal(int16_t rssi) {
  if (!selectedEverSeen) {
    for (uint8_t i = 0; i < BLE_RSSI_SAMPLES; i++) {
      rssiSamples[i] = rssi;
    }
    selectedEverSeen = true;
    rssiSampleIndex = 0;
  } else {
    rssiSamples[rssiSampleIndex] = rssi;
    rssiSampleIndex = (uint8_t)((rssiSampleIndex + 1) % BLE_RSSI_SAMPLES);
  }
  selectedLastSeenMillis = millis();
  setParameter(PARAM_BLE_RSSI, getMedianInt11(rssiSamples));
}

/* Keeps the table on the devices that are actually around: when it is full the
   one heard longest ago makes way, so a busy place does not freeze the list on
   whatever happened to advertise first. */
static BleDevice* deviceSlotFor(const char* address) {
  for (uint8_t i = 0; i < deviceCount; i++) {
    if (strcasecmp(devices[i].address, address) == 0) {
      return &devices[i];
    }
  }
  if (deviceCount < BLE_MAX_DEVICES) {
    BleDevice* slot = &devices[deviceCount++];
    memset(slot, 0, sizeof(BleDevice));
    strncpy(slot->address, address, BLE_ADDRESS_LENGTH - 1);
    return slot;
  }

  BleDevice* oldest = &devices[0];
  for (uint8_t i = 1; i < deviceCount; i++) {
    if ((int32_t)(devices[i].lastSeenMillis - oldest->lastSeenMillis) < 0) {
      oldest = &devices[i];
    }
  }
  memset(oldest, 0, sizeof(BleDevice));
  strncpy(oldest->address, address, BLE_ADDRESS_LENGTH - 1);
  return oldest;
}

class BleScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertised) override {
    /* the scan keeps no results of its own, so this object is freed as soon as
       the callback returns - everything needed has to be copied out here */
    char address[BLE_ADDRESS_LENGTH];
    strncpy(address, advertised->getAddress().toString().c_str(),
            BLE_ADDRESS_LENGTH - 1);
    address[BLE_ADDRESS_LENGTH - 1] = '\0';
    int16_t rssi = (int16_t)advertised->getRSSI();

    if (!bleTake()) {
      return;
    }
    BleDevice* device = deviceSlotFor(address);
    if (advertised->haveName()) {
      strncpy(device->name, advertised->getName().c_str(), BLE_NAME_LENGTH - 1);
      device->name[BLE_NAME_LENGTH - 1] = '\0';
    }
    device->addressType = advertised->getAddressType();
#if CONFIG_BT_NIMBLE_EXT_ADV
    device->legacy = advertised->isLegacyAdvertisement();
    device->primaryPhy = advertised->getPrimaryPhy();
#endif
    if (advertised->haveTXPower()) {
      device->txPower = advertised->getTXPower();
      device->haveTxPower = true;
    }
    device->rssi = rssi;
    device->lastSeenMillis = millis();
    if (device->advertisements == 0 || rssi > device->bestRssi) {
      device->bestRssi = rssi;
    }
    if (device->advertisements < 65535) {
      device->advertisements++;
    }

    if (selectedAddress[0] != '\0' &&
        strcasecmp(selectedAddress, address) == 0) {
      recordBeaconSignal(rssi);
    }
    bleGive();
  }
};

static BleScanCallbacks scanCallbacks;

/* getParameter(key, value) strcpy's whatever NVS holds, so the landing buffer
   is sized for a value this task did not write rather than for the 17
   characters it does. */
static void loadSelection() {
  char stored[64] = "";
  getParameter(BLE_SELECTION_KEY, stored);
  stored[sizeof(stored) - 1] = '\0';
  strncpy(selectedAddress, stored, BLE_ADDRESS_LENGTH - 1);
  selectedAddress[BLE_ADDRESS_LENGTH - 1] = '\0';
}

/* Whether the device is expected to change this address under us.

   A public address never does. A random one says which kind it is in the top
   two bits of its first byte: 11 is static and keeps until the device reboots,
   01 and 00 are private and are meant to change - a quarter of an hour on most
   phones. The distinction is worth making rather than calling everything random
   unstable, because a static random address is perfectly good to track, and
   telling an operator otherwise sends them looking for a tag they already have.
 */
static boolean addressRotates(const char* address, uint8_t addressType) {
  if (addressType == BLE_ADDRESS_TYPE_PUBLIC) {
    return false;
  }
  char first[3] = {address[0], address[1], '\0'};
  return ((uint8_t)strtol(first, NULL, 16) & 0xC0) != 0xC0;
}

static void printAge(Print* output, uint32_t lastSeenMillis) {
  output->print((millis() - lastSeenMillis) / 1000);
  output->print(F(" s ago"));
}

/* The log-distance path loss model, which is the only one a single receiver can
   run: rssi = reference - 10 n log10(d), so d = 10 ^ ((reference - rssi) /
   10n). With the exponent stored ten times too large the 10 n cancels into it.

   It is an estimate of an estimate. RSSI through a hedge reads like RSSI across
   twice the open field, and a tag on a hornet turns its own body between the
   antenna and here several times a second - which is what the median is for.
   Treat it as "warmer or colder", not as a measurement: the number that finds a
   nest is the one that keeps falling as you walk. */
static uint8_t pathLossTimesTen() {
  int16_t value = getParameter(PARAM_BLE_PATH_LOSS);
  if (value < 10 || value > 60) {
    return BLE_PATH_LOSS_DEFAULT;
  }
  return (uint8_t)value;
}

/* Negative when no distance can be claimed, never a guessed one. */
static double estimateMetres(int16_t rssi) {
  int16_t reference = getParameter(PARAM_BLE_REFERENCE_RSSI);
  if (reference == ERROR_VALUE || rssi == ERROR_VALUE) {
    return -1.0;
  }
  return pow(10.0, (double)(reference - rssi) / (double)pathLossTimesTen());
}

static void printDistance(Print* output, int16_t rssi) {
  double metres = estimateMetres(rssi);
  if (metres < 0.0) {
    return;
  }
  output->print(F(", ~"));
  if (metres < 10.0) {
    output->print(metres, 1);
  } else {
    output->print((int32_t)(metres + 0.5));
  }
  output->print(F(" m"));
}

static void printBleList(Print* output) {
  if (!bleTake()) {
    output->println(F("Bluetooth busy"));
    return;
  }
  if (deviceCount == 0) {
    output->println(F("No device heard yet"));
  }
  for (uint8_t i = 0; i < deviceCount; i++) {
    BleDevice* device = &devices[i];
    output->print(strcasecmp(device->address, selectedAddress) == 0 ? '*'
                                                                    : ' ');
    output->print(i);
    output->print(' ');
    output->print(device->address);
    output->print(' ');
    output->print(device->rssi);
    output->print(F(" dBm"));
    printDistance(output, device->rssi);
    output->print(F(", "));
    output->print(device->advertisements);
    output->print(F(" adv, "));
    printAge(output, device->lastSeenMillis);
    if (device->name[0] != '\0') {
      output->print(F(", "));
      output->print(device->name);
    }
    output->println();
  }
  bleGive();
}

static void printBleInfo(Print* output) {
  output->println(F("=== Bluetooth ==="));
  output->print(F("Scanning: "));
  output->println(bleScan != NULL && bleScan->isScanning() ? F("yes")
                                                           : F("no"));

  if (!bleTake()) {
    output->println(F("Bluetooth busy"));
    return;
  }
  output->print(F("Devices heard: "));
  output->println(deviceCount);
  output->print(F("Selected: "));
  if (selectedAddress[0] == '\0') {
    output->println(F("none - pick one with bl then bs<index>"));
  } else {
    output->println(selectedAddress);
    output->print(F("Signal: "));
    if (!selectedEverSeen) {
      output->println(F("not heard"));
    } else {
      output->print(getParameter(PARAM_BLE_RSSI));
      output->print(F(" dBm"));
      printDistance(output, getParameter(PARAM_BLE_RSSI));
      output->print(F(" ("));
      printAge(output, selectedLastSeenMillis);
      output->println(')');
    }
    for (uint8_t i = 0; i < deviceCount; i++) {
      if (strcasecmp(devices[i].address, selectedAddress) == 0) {
        output->print(F("Address type: "));
        if (devices[i].addressType == BLE_ADDRESS_TYPE_PUBLIC) {
          output->println(F("public - the first 3 bytes are an IEEE OUI"));
        } else if (addressRotates(devices[i].address, devices[i].addressType)) {
          output->println(F("private - no manufacturer, and it will rotate"));
        } else {
          output->println(F("random static - no manufacturer, but it keeps"));
        }
        output->print(F("Tag TX power: "));
        if (devices[i].haveTxPower) {
          output->print(devices[i].txPower);
          output->println(F(" dBm advertised"));
        } else {
          output->println(F("not advertised"));
        }
        break;
      }
    }
  }
  bleGive();

  output->print(F("Range model: "));
  int16_t reference = getParameter(PARAM_BLE_REFERENCE_RSSI);
  if (reference == ERROR_VALUE) {
    output->println(F("uncalibrated - hold the tag at 1 m and type bk"));
  } else {
    output->print(reference);
    output->print(F(" dBm at 1 m, exponent "));
    output->println(pathLossTimesTen() / 10.0, 1);
  }

  output->print(F("Reported as "));
  output->print(numberToLabel(PARAM_BLE_RSSI));
#ifdef PARAM_LORA_INTERVAL_SECONDS
  output->print(F(", broadcast every "));
  output->print(getParameter(PARAM_LORA_INTERVAL_SECONDS));
  output->print(F(" s ("));
  output->print(numberToLabel(PARAM_LORA_INTERVAL_SECONDS));
  output->println(')');

  /* the likeliest way this ends up silent: a board flashed over a tracker that
     was configured before it could scan keeps that window, which stops one slot
     short of the reading and looks like a beacon nobody can hear */
  int16_t first = getParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER);
  int16_t count = getParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS);
  if (first > PARAM_BLE_RSSI || first + count <= PARAM_BLE_RSSI) {
    output->print(F("Not in the broadcast window - gt"));
    int16_t interval = getParameter(PARAM_LORA_INTERVAL_SECONDS);
    output->print(interval > 0 ? interval : 60);
    output->println(F(" fixes it"));
  }
#else
  output->println();
#endif
}

#ifdef THR_LORA_MESH
/* The feed a listening bridge emits: one line per device heard since the last
   sweep, which is how the operator finds a tag at all. Hold the tag against the
   bridge, take the address off the strongest line, and send it to the tracker
   with ar<node>:bs<address>.

   Per sweep and not per advertisement on purpose. One device alone can produce
   hundreds a minute, and two dozen of them would saturate the port and starve
   the mesh's own JSON - the host would be reading Bluetooth while the packets
   it exists to record went unwritten. */
static uint32_t lastReportMillis = 0;

static uint32_t reportIntervalMillis() {
  int16_t value = getParameter(PARAM_BLE_REPORT_SECONDS);
  if (value == ERROR_VALUE) {
    return BLE_REPORT_SECONDS_DEFAULT * 1000ul;
  }
  if (value <= 0) {
    return 0;
  }
  return (uint32_t)value * 1000ul;
}

/* One entry is copied out under the mutex and printed outside it: a sweep is
   several kilobytes at 115200 baud, and holding the lock across that would
   stall the scan callback for as long as it takes to print. */
static void reportDevices() {
  if (!loraMeshIsBridge()) {
    return;
  }
  for (uint8_t i = 0; i < BLE_MAX_DEVICES; i++) {
    if (!bleTake()) {
      return;
    }
    if (i >= deviceCount) {
      bleGive();
      return;
    }
    BleDevice device = devices[i];
    devices[i].advertisements = 0;
    bleGive();

    /* nothing heard in this window has nothing to report; the entry stays in
       the table so its address keeps the index (bs3) it was listed under */
    if (device.advertisements == 0) {
      continue;
    }

    Print* json = loraBridgeBegin("ble");
    loraBridgeTextBytes(json, "addr", device.address,
                        (uint8_t)strlen(device.address));
    loraBridgeInt(json, "rssi", device.rssi);
    loraBridgeInt(json, "best", device.bestRssi);
    loraBridgeInt(json, "adv", device.advertisements);
    loraBridgeInt(json, "type", device.addressType);
#if CONFIG_BT_NIMBLE_EXT_ADV
    /* 1 = 1M, 2 = 2M, 3 = Coded. A device only ever heard on 3 is a long range
       advertiser that a legacy scan cannot reach at any distance. */
    loraBridgeInt(json, "phy", device.primaryPhy);
    if (!device.legacy) {
      loraBridgeInt(json, "ext", 1);
    }
#endif
    if (device.name[0] != '\0') {
      loraBridgeTextBytes(json, "name", device.name,
                          (uint8_t)strlen(device.name));
    }
    if (device.haveTxPower) {
      loraBridgeInt(json, "tx", device.txPower);
    }
    loraBridgeEnd(json);
  }
}
#endif

/* An index is what the operator just read off (bl), an address is what survives
   a reboot - so the index is resolved to an address here and never stored. */
static void processBleSelect(char* paramValue, Print* output) {
  if (paramValue[0] == '\0') {
    output->println(selectedAddress[0] == '\0' ? "none" : selectedAddress);
    return;
  }

  char address[BLE_ADDRESS_LENGTH];
  if (strchr(paramValue, ':') != NULL) {
    /* a mistyped address is accepted by every comparison and matches nothing,
       so the shape is checked here rather than looking like a beacon that is
       simply never in range */
    if (strlen(paramValue) != BLE_ADDRESS_LENGTH - 1) {
      output->println(F("Expected an address like aa:bb:cc:dd:ee:ff"));
      return;
    }
    strncpy(address, paramValue, BLE_ADDRESS_LENGTH - 1);
    address[BLE_ADDRESS_LENGTH - 1] = '\0';
  } else {
    int index = atoi(paramValue);
    if (!bleTake()) {
      output->println(F("Bluetooth busy"));
      return;
    }
    if (index < 0 || index >= deviceCount) {
      bleGive();
      output->println(F("No such device, bl lists them"));
      return;
    }
    strncpy(address, devices[index].address, BLE_ADDRESS_LENGTH - 1);
    address[BLE_ADDRESS_LENGTH - 1] = '\0';
    bleGive();
  }

  if (!bleTake()) {
    output->println(F("Bluetooth busy"));
    return;
  }
  strncpy(selectedAddress, address, BLE_ADDRESS_LENGTH - 1);
  selectedAddress[BLE_ADDRESS_LENGTH - 1] = '\0';
  forgetBeaconSignal();
  bleGive();

  setParameter(BLE_SELECTION_KEY, address);
  output->println(address);

  /* Selecting an address the device rotates tracks it until the next rotation -
     a quarter of an hour on most phones - and then reports nothing, with
     nothing looking wrong. Said once, here, because there is no later moment at
     which the node could notice. */
  if (!bleTake()) {
    return;
  }
  for (uint8_t i = 0; i < deviceCount; i++) {
    if (strcasecmp(devices[i].address, address) != 0) {
      continue;
    }
    uint8_t type = devices[i].addressType;
    bleGive();
    if (addressRotates(address, type)) {
      output->println(F("Warning: private address, it will rotate and this"));
      output->println(
          F("will then track nothing. Use a tag with a fixed one."));
    }
    return;
  }
  bleGive();
}

/* (bk) is the whole reason a distance can be shown at all: it measures the tag
   in hand instead of assuming it. Exactly one metre is awkward to set up in a
   field, so (bk500) calibrates at a paced five metres and solves the same model
   backwards for the reference. Calibrate through the air you will search in -
   the same tag reads several dB lower in a hedge than over grass. */
static void processBleCalibrate(char* paramValue, Print* output) {
  if (!bleTake()) {
    output->println(F("Bluetooth busy"));
    return;
  }
  boolean heard = selectedEverSeen;
  int16_t rssi = getParameter(PARAM_BLE_RSSI);
  bleGive();

  if (!heard || rssi == ERROR_VALUE) {
    output->println(F("No signal to calibrate against - bs the tag first"));
    return;
  }

  double centimetres = paramValue[0] == '\0' ? 100.0 : atof(paramValue);
  if (centimetres < 10.0) {
    output->println(F("Calibrate at 10 cm or more, e.g. bk500 at 5 m"));
    return;
  }

  int16_t reference =
      (int16_t)lround(rssi + pathLossTimesTen() * log10(centimetres / 100.0));
  setAndSaveParameter(PARAM_BLE_REFERENCE_RSSI, reference);

  output->print(F("Calibrated: "));
  output->print(rssi);
  output->print(F(" dBm at "));
  output->print(centimetres / 100.0, 2);
  output->print(F(" m -> "));
  output->print(reference);
  output->println(F(" dBm at 1 m"));
}

static void processBleForget(Print* output) {
  if (!bleTake()) {
    output->println(F("Bluetooth busy"));
    return;
  }
  selectedAddress[0] = '\0';
  forgetBeaconSignal();
  bleGive();

  deleteParameter(BLE_SELECTION_KEY);
  output->println(F("Selection cleared"));
}

static void processBleClear(Print* output) {
  if (!bleTake()) {
    output->println(F("Bluetooth busy"));
    return;
  }
  deviceCount = 0;
  bleGive();

  if (bleScan != NULL) {
    bleScan->clearDuplicateCache();
  }
  output->println(F("Device list cleared"));
}

void processBleCommand(char command, char* paramValue, Print* output) {
  switch (command) {
    case 'i':
      printBleInfo(output);
      break;
    case 'l':
      printBleList(output);
      break;
    case 's':
      processBleSelect(paramValue, output);
      break;
    case 'c':
      processBleClear(output);
      break;
    case 'k':
      processBleCalibrate(paramValue, output);
      break;
    case 'z':
      processBleForget(output);
      break;
    default:
      output->println(F("(bi) info - selection, signal, range model"));
      output->println(F("(bl) list the devices heard"));
      output->println(
          F("(bs) monitor one of them, bs3 or bsaa:bb:cc:dd:ee:ff"));
      output->println(F("(bk) calibrate the range: bk at 1 m, bk500 at 5 m"));
      output->println(F("(bc) clear the device list"));
      output->println(F("(bz) stop monitoring, forget the selection"));
      printParameterHelp(output, PARAM_BLE_RSSI,
                         F("median RSSI in dBm, unset = out of range"));
      printParameterHelp(output, PARAM_BLE_REFERENCE_RSSI,
                         F("dBm at 1 m, per tag model - set it with bk"));
      printParameterHelp(output, PARAM_BLE_PATH_LOSS,
                         F("path loss x10: 20 open, 25-35 through cover"));
      printParameterHelp(output, PARAM_BLE_REPORT_SECONDS,
                         F("seconds between JSON sweeps on a bridge, 0 = off"));
      break;
  }
}

void TaskBLE(void* pvParameters) {
  (void)pvParameters;

  bleMutex = xSemaphoreCreateMutex();
  setParameter(PARAM_BLE_RSSI, ERROR_VALUE);
  loadSelection();

  NimBLEDevice::init("");
  bleScan = NimBLEDevice::getScan();
  /* an active scan asks for the scan response, which is where most beacons put
     their name - without it (bl) is a list of addresses */
  bleScan->setActiveScan(true);
  bleScan->setInterval(BLE_SCAN_INTERVAL_MS);
  bleScan->setWindow(BLE_SCAN_INTERVAL_MS);
  /* 0 keeps no results: the table below is the record, and NimBLE's own would
     grow for as long as the scan runs, which is forever */
  bleScan->setMaxResults(0);
  bleScan->setAdvertisedDeviceCallbacks(&scanCallbacks, true);

  Serial.println(F("BLE task started. Use 'bl' to list what is around."));
  if (selectedAddress[0] != '\0') {
    Serial.print(F("[BLE] monitoring "));
    Serial.println(selectedAddress);
  }

  while (true) {
    /* duration 0 scans until stopped, but the host resets on its own errors and
       comes back not scanning, and a tracker nobody can plug a cable into has
       to notice that itself */
    if (!bleScan->isScanning()) {
      bleScan->start(0, NULL, false);
    }

    if (bleTake()) {
      if (selectedEverSeen &&
          millis() - selectedLastSeenMillis > (uint32_t)BLE_BEACON_TIMEOUT_MS) {
        forgetBeaconSignal();
      }
      bleGive();
    }

#ifdef THR_LORA_MESH
    uint32_t interval = reportIntervalMillis();
    if (interval > 0 && millis() - lastReportMillis >= interval) {
      lastReportMillis = millis();
      reportDevices();
    }
#endif

    vTaskDelay(1000);
  }
}

void taskBLE() {
  xTaskCreatePinnedToCore(TaskBLE, "TaskBLE",
                          4096,  // Crash if less than 4096 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          1,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif
