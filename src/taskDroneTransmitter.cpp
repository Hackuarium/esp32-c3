#include "config.h"
#ifdef THR_DRONE_TRANSMITTER
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <string.h>

#include "droneId/droneIdTransmit.h"
#include "params.h"

#if !CONFIG_BT_NIMBLE_EXT_ADV
#error "The drone transmitter needs -D CONFIG_BT_NIMBLE_EXT_ADV=1 in the env: \
Bluetooth 5 Long Range is an extended advertisement on the coded PHY."
#endif

/* A known aircraft for the watcher to hear, announced every second while it
   flies a circle around a fixed point.

   Bluetooth 4 has room for one message per advertisement, so the five take
   turns, one every 200 ms - each type once a second, which is the rate the
   standard asks of a position. Bluetooth 5 and the Wi-Fi beacon carry all five
   in one pack, rebuilt once a second. The radio repeats whatever it holds
   between two updates, at the advertising intervals below. */
#define DRONE_TX_SLOT_MS 200
#define DRONE_TX_SLOTS_PER_SECOND 5
#define DRONE_TX_LEGACY_INTERVAL_MS 100
#define DRONE_TX_LONG_RANGE_INTERVAL_MS 200
/* the beacon interval field, in 1.024 ms units: once a second */
#define DRONE_TX_BEACON_INTERVAL_TU 977

/* each advertising set has its own random static address, as the reference
   transmitter does, so a watcher files BT4 and BT5 as two rows */
#define LEGACY_INSTANCE 0
#define LONG_RANGE_INSTANCE 1

static const ODID_messagetype_t rotation[DRONE_TX_SLOTS_PER_SECOND] = {
    ODID_MESSAGETYPE_BASIC_ID, ODID_MESSAGETYPE_LOCATION,
    ODID_MESSAGETYPE_SELF_ID, ODID_MESSAGETYPE_SYSTEM,
    ODID_MESSAGETYPE_OPERATOR_ID};

static ODID_UAS_Data aircraft;
static char uasId[ODID_ID_SIZE + 1];
static char wifiPassword[17];
static uint8_t wifiAddress[6];
static NimBLEAddress legacyAddress;
static NimBLEAddress longRangeAddress;

/* what is on the air now, compared against the parameters every slot */
static int16_t appliedLegacy = 0;
static int16_t appliedLongRange = 0;
static int16_t appliedChannel = 0;

/* the standard counts per message type on Bluetooth 4, per pack elsewhere */
static uint8_t legacyCounters[DRONE_TX_SLOTS_PER_SECOND];
static uint8_t longRangeCounter = 0;
static uint8_t beaconCounter = 0;

static uint32_t legacySent = 0;
static uint32_t legacyRefused = 0;
static uint32_t longRangeSent = 0;
static uint32_t longRangeRefused = 0;
static uint32_t beaconsSent = 0;
static uint32_t beaconsRefused = 0;

static uint8_t frame[256];

void resetParameters();

/* A board keeps whatever the firmware it last ran stored, and there A, B and
   C mean something else: the drone tracker's A7 B3 C6 are seconds and a
   channel, which read here as both Bluetooth transports off. The qualifier
   resetParameters() writes is what says the block is ours, and a blank NVS
   reads it as 0, so one test covers a new board and a recycled one. */
static void resetIfNotOurs() {
  if (getQualifier() != DRONE_TX_QUALIFIER) {
    resetParameters();
  }
}

static boolean isOn(uint8_t parameter) {
  return getParameter(parameter) == 1;
}

/* 1 to 13, anything else is off */
static uint8_t wifiChannel() {
  int16_t channel = getParameter(PARAM_DRONE_TX_CHANNEL);
  return channel >= 1 && channel <= 13 ? (uint8_t)channel : 0;
}

/* A CTA-2063-A serial number: manufacturer code, a length code (C is twelve)
   and twelve characters from its alphabet, which leaves out O and I. TEST is
   no manufacturer's code. Drawn after the radio is up, since esp_random() is
   only a true random source while RF is running. */
static void drawSerialNumber() {
  static const char alphabet[] = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";
  memcpy(uasId, "TESTC", 5);
  for (uint8_t i = 5; i < 17; i++) {
    uasId[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
  }
  uasId[17] = '\0';

  static const char hex[] = "0123456789abcdef";
  for (uint8_t i = 0; i < 16; i++) {
    wifiPassword[i] = hex[esp_random() & 0x0F];
  }
  wifiPassword[16] = '\0';
}

static void describeAircraft() {
  odid_initUasData(&aircraft);

  aircraft.BasicIDValid[0] = 1;
  aircraft.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
  aircraft.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
  strncpy(aircraft.BasicID[0].UASID, uasId, ODID_ID_SIZE);

  /* No clock on this board, so the timestamp says unknown rather than
     inventing one. The position, course and speed are updatePosition()'s. */
  aircraft.LocationValid = 1;
  aircraft.Location.Status = ODID_STATUS_AIRBORNE;
  aircraft.Location.AltitudeGeo = DRONE_TX_GROUND_ALTITUDE + DRONE_TX_HEIGHT;
  aircraft.Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
  aircraft.Location.Height = DRONE_TX_HEIGHT;
  aircraft.Location.SpeedVertical = 0;
  aircraft.Location.HorizAccuracy = ODID_HOR_ACC_10_METER;
  aircraft.Location.VertAccuracy = ODID_VER_ACC_10_METER;
  aircraft.Location.TimeStamp = INV_TIMESTAMP;

  /* what a phone app shows first, so nobody mistakes it for a real flight */
  aircraft.SelfIDValid = 1;
  aircraft.SelfID.DescType = ODID_DESC_TYPE_TEXT;
  strncpy(aircraft.SelfID.Desc, "hackuarium test beacon", ODID_STR_SIZE);

  /* where the operator stands is updatePosition()'s too */
  aircraft.SystemValid = 1;
  aircraft.System.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
  aircraft.System.OperatorAltitudeGeo = DRONE_TX_GROUND_ALTITUDE;
  aircraft.System.CategoryEU = ODID_CATEGORY_EU_OPEN;
  aircraft.System.ClassEU = ODID_CLASS_EU_CLASS_0;

  aircraft.OperatorIDValid = 1;
  aircraft.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
  strncpy(aircraft.OperatorID.OperatorId, "CHEhackuarium000", ODID_ID_SIZE);
}

static void updateOperator(double seconds) {
  ODID_System_data* system = &aircraft.System;
  if (DRONE_TX_OPERATOR_SPEED <= 0) {
    system->OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_FIXED;
    system->OperatorLatitude = DRONE_TX_LATITUDE;
    system->OperatorLongitude = DRONE_TX_LONGITUDE;
    return;
  }
  float course;
  system->OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_LIVE_GNSS;
  droneIdOrbitPosition(DRONE_TX_LATITUDE, DRONE_TX_LONGITUDE,
                       DRONE_TX_OPERATOR_DIAMETER / 2, DRONE_TX_OPERATOR_SPEED,
                       seconds, &system->OperatorLatitude,
                       &system->OperatorLongitude, &course);
}

/* Where the aircraft and its operator are on their circles now, from the time
   since boot - the only time this board has. */
static void updatePosition() {
  double seconds = esp_timer_get_time() / 1e6;
  updateOperator(seconds);
  ODID_Location_data* location = &aircraft.Location;
  if (DRONE_TX_SPEED <= 0) {
    location->Latitude = DRONE_TX_LATITUDE;
    location->Longitude = DRONE_TX_LONGITUDE;
    location->SpeedHorizontal = 0;
    location->Direction = INV_DIR;
    return;
  }
  droneIdOrbitPosition(DRONE_TX_LATITUDE, DRONE_TX_LONGITUDE,
                       DRONE_TX_ORBIT_DIAMETER / 2, DRONE_TX_SPEED, seconds,
                       &location->Latitude, &location->Longitude,
                       &location->Direction);
  location->SpeedHorizontal = DRONE_TX_SPEED;
}

/* the two most significant bits set is what makes it random static */
static NimBLEAddress randomStaticAddress() {
  uint8_t address[6];
  uint32_t high = esp_random();
  uint32_t low = esp_random();
  memcpy(address, &high, 4);
  memcpy(&address[4], &low, 2);
  address[0] |= 0xC0;
  return NimBLEAddress(address, BLE_ADDR_RANDOM);
}

static uint32_t intervalUnits(uint32_t milliseconds) {
  return milliseconds * 8 / 5;  // 0.625 ms units
}

static boolean startAdvertising(uint8_t instance,
                                uint8_t phy,
                                uint32_t intervalMs,
                                const NimBLEAddress& address,
                                const uint8_t* data,
                                size_t length) {
  NimBLEExtAdvertisement advertisement(phy, phy);
  /* legacy PDUs exist only on 1M, and are what a phone can read */
  advertisement.setLegacyAdvertising(phy == BLE_HCI_LE_PHY_1M);
  advertisement.setConnectable(false);
  advertisement.setScannable(false);
  advertisement.setMinInterval(intervalUnits(intervalMs));
  advertisement.setMaxInterval(intervalUnits(intervalMs));
  advertisement.setAddress(address);
  advertisement.setData(data, length);
  NimBLEExtAdvertising* advertising = NimBLEDevice::getAdvertising();
  return advertising->setInstanceData(instance, advertisement) &&
         advertising->start(instance);
}

/* NimBLE-Arduino only sets data by reconfiguring the set, which the host
   refuses while it advertises. The host call underneath replaces the payload
   of a running set, which is what a rotation every 200 ms needs. */
static boolean replaceAdvertisement(uint8_t instance,
                                    const uint8_t* data,
                                    size_t length) {
  struct os_mbuf* buffer = os_msys_get_pkthdr(length, 0);
  if (buffer == NULL) {
    return false;
  }
  if (os_mbuf_append(buffer, data, length) != 0) {
    os_mbuf_free_chain(buffer);
    return false;
  }
  return ble_gap_ext_adv_set_data(instance, buffer) == 0;  // frees the buffer
}

/* Replaces the payload of a running set, or starts one that is not running.
   A refusal stops the set, so one the host has forgotten - it resets on its
   own errors and comes back idle - is configured afresh on the next turn. */
static boolean advertise(uint8_t instance,
                         uint8_t phy,
                         uint32_t intervalMs,
                         const NimBLEAddress& address,
                         const uint8_t* data,
                         size_t length) {
  NimBLEExtAdvertising* advertising = NimBLEDevice::getAdvertising();
  boolean sent =
      length > 0 &&
      (advertising->isActive(instance)
           ? replaceAdvertisement(instance, data, length)
           : startAdvertising(instance, phy, intervalMs, address, data, length));
  if (!sent) {
    advertising->stop(instance);
  }
  return sent;
}

static void sendLegacy(uint8_t slot) {
  size_t length = droneIdBuildLegacyAdvertisement(
      &aircraft, rotation[slot], legacyCounters[slot]++, frame);
  if (advertise(LEGACY_INSTANCE, BLE_HCI_LE_PHY_1M,
                DRONE_TX_LEGACY_INTERVAL_MS, legacyAddress, frame, length)) {
    legacySent++;
  } else {
    legacyRefused++;
  }
}

static void sendLongRange() {
  size_t length = droneIdBuildExtendedAdvertisement(
      &aircraft, longRangeCounter++, frame, sizeof(frame));
  if (advertise(LONG_RANGE_INSTANCE, BLE_HCI_LE_PHY_CODED,
                DRONE_TX_LONG_RANGE_INTERVAL_MS, longRangeAddress, frame,
                length)) {
    longRangeSent++;
  } else {
    longRangeRefused++;
  }
}

/* A soft AP only so the driver will transmit: hidden, locked with a password
   nobody knows, and its own beacons slowed to one a minute so they do not
   crowd the channel the frames that matter are sent on by hand. */
static boolean startWifi(uint8_t channel) {
  WiFi.persistent(false);
  if (!WiFi.softAP(uasId, wifiPassword, channel, 1, 1)) {
    return false;
  }
  wifi_config_t config;
  if (esp_wifi_get_config(WIFI_IF_AP, &config) == ESP_OK) {
    config.ap.beacon_interval = 60000;
    esp_wifi_set_config(WIFI_IF_AP, &config);
  }
  if (esp_wifi_set_max_tx_power(DRONE_TX_WIFI_POWER) != ESP_OK) {
    return false;
  }
  return esp_wifi_get_mac(WIFI_IF_AP, wifiAddress) == ESP_OK;
}

static void stopWifi() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

static void sendBeacon() {
  size_t length = droneIdBuildBeaconFrame(
      &aircraft, wifiAddress, uasId, DRONE_TX_BEACON_INTERVAL_TU,
      beaconCounter++, frame, sizeof(frame));
  boolean sent = length > 0 && esp_wifi_80211_tx(WIFI_IF_AP, frame, length,
                                                 true) == ESP_OK;
  if (sent) {
    beaconsSent++;
  } else {
    beaconsRefused++;
  }
}

static void printSummary(Print* output) {
  output->print(F("[drone tx] "));
  output->print(uasId);
  output->print(DRONE_TX_SPEED > 0 ? F(" circling ") : F(" over "));
  output->print(DRONE_TX_LATITUDE, 7);
  output->print(F(", "));
  output->print(DRONE_TX_LONGITUDE, 7);
  output->print(appliedLegacy ? F(" BT4") : F(""));
  output->print(appliedLongRange ? F(" BT5") : F(""));
  if (appliedChannel) {
    output->print(F(" beacon ch"));
    output->print(appliedChannel);
  }
  if (!appliedLegacy && !appliedLongRange && !appliedChannel) {
    output->print(F(" - silent"));
  }
  output->println();
}

/* Transports are switched on lazily: the next send of one that is off in the
   radio but on in the parameters starts it. */
static void applySettings() {
  NimBLEExtAdvertising* advertising = NimBLEDevice::getAdvertising();
  boolean changed = false;

  if (appliedLegacy != isOn(PARAM_DRONE_TX_LEGACY)) {
    appliedLegacy = isOn(PARAM_DRONE_TX_LEGACY);
    if (!appliedLegacy) {
      advertising->stop(LEGACY_INSTANCE);
    }
    changed = true;
  }
  if (appliedLongRange != isOn(PARAM_DRONE_TX_LONG_RANGE)) {
    appliedLongRange = isOn(PARAM_DRONE_TX_LONG_RANGE);
    if (!appliedLongRange) {
      advertising->stop(LONG_RANGE_INSTANCE);
    }
    changed = true;
  }
  if (appliedChannel != wifiChannel()) {
    appliedChannel = wifiChannel();
    if (appliedChannel == 0) {
      stopWifi();
    } else if (!startWifi(appliedChannel)) {
      Serial.println(F("[drone tx] the Wi-Fi driver refused the soft AP"));
    }
    changed = true;
  }

  if (changed) {
    printSummary(&Serial);
  }
}

static void printTransport(Print* output,
                           const __FlashStringHelper* name,
                           boolean on,
                           const char* address,
                           uint32_t sent,
                           uint32_t refused,
                           const __FlashStringHelper* unit) {
  output->print(name);
  if (!on) {
    output->println(F("off"));
    return;
  }
  output->print(address);
  output->print(F(", "));
  output->print(sent);
  output->print(unit);
  if (refused > 0) {
    output->print(F(", "));
    output->print(refused);
    output->print(F(" refused"));
  }
  output->println();
}

static void printInfo(Print* output) {
  output->println(F("=== Drone transmitter ==="));
  output->print(F("UAS ID: "));
  output->print(uasId);
  output->println(F(" (serial number), multirotor"));
  output->print(F("Position: "));
  output->print(aircraft.Location.Latitude, 7);
  output->print(F(", "));
  output->print(aircraft.Location.Longitude, 7);
  output->print(F(", "));
  output->print(DRONE_TX_GROUND_ALTITUDE + DRONE_TX_HEIGHT, 1);
  output->print(F(" m geodetic, "));
  output->print(DRONE_TX_HEIGHT, 1);
  output->println(F(" m above take-off"));
  if (DRONE_TX_SPEED > 0) {
    output->print(F("Flying: a "));
    output->print(DRONE_TX_ORBIT_DIAMETER, 0);
    output->print(F(" m circle, clockwise, at "));
    output->print(DRONE_TX_SPEED, 1);
    output->print(F(" m/s, course "));
    output->print(aircraft.Location.Direction, 0);
    output->print(F(", one lap every "));
    output->print(M_PI * DRONE_TX_ORBIT_DIAMETER / DRONE_TX_SPEED, 0);
    output->println(F(" s"));
  } else {
    output->println(F("Flying: hovering"));
  }
  output->print(F("Operator: "));
  output->print(aircraft.System.OperatorLatitude, 7);
  output->print(F(", "));
  output->print(aircraft.System.OperatorLongitude, 7);
  if (DRONE_TX_OPERATOR_SPEED > 0) {
    output->print(F(", walking a "));
    output->print(DRONE_TX_OPERATOR_DIAMETER, 0);
    output->print(F(" m circle at "));
    output->print(DRONE_TX_OPERATOR_SPEED, 1);
    output->println(F(" m/s (live GNSS)"));
  } else {
    output->println(F(", standing (fixed)"));
  }

  char wifi[18];
  snprintf(wifi, sizeof(wifi), "%02x:%02x:%02x:%02x:%02x:%02x", wifiAddress[0],
           wifiAddress[1], wifiAddress[2], wifiAddress[3], wifiAddress[4],
           wifiAddress[5]);
  printTransport(output, F("BT4 legacy (A): "), appliedLegacy,
                 legacyAddress.toString().c_str(), legacySent, legacyRefused,
                 F(" messages"));
  printTransport(output, F("BT5 long range (B): "), appliedLongRange,
                 longRangeAddress.toString().c_str(), longRangeSent,
                 longRangeRefused, F(" packs"));
  output->print(F("Wi-Fi beacon (C): "));
  if (appliedChannel) {
    output->print(F("channel "));
    output->print(appliedChannel);
    output->print(F(", "));
  }
  printTransport(output, F(""), appliedChannel, wifi, beaconsSent,
                 beaconsRefused, F(" beacons"));
}

void processDroneCommand(char command, char* paramValue, Print* output) {
  (void)paramValue;
  switch (command) {
    case 'i':
      printInfo(output);
      break;
    default:
      output->println(F("(di) info - identity, position, what is sent"));
      printParameterHelp(output, PARAM_DRONE_TX_LEGACY,
                         F("Bluetooth 4 legacy, 1 = on, 0 = off"));
      printParameterHelp(output, PARAM_DRONE_TX_LONG_RANGE,
                         F("Bluetooth 5 Long Range, 1 = on, 0 = off"));
      printParameterHelp(output, PARAM_DRONE_TX_CHANNEL,
                         F("Wi-Fi beacon channel 1 to 13, 0 = off"));
      break;
  }
}

void TaskDroneTransmitter(void* pvParameters) {
  (void)pvParameters;

  resetIfNotOurs();

  NimBLEDevice::init("");
  drawSerialNumber();
  describeAircraft();
  legacyAddress = randomStaticAddress();
  longRangeAddress = randomStaticAddress();

  uint8_t slot = 0;
  TickType_t wake = xTaskGetTickCount();
  while (true) {
    applySettings();

    if (slot == 0) {
      updatePosition();
    }
    if (appliedLegacy) {
      sendLegacy(slot);
    }
    if (slot == 0) {
      if (appliedLongRange) {
        sendLongRange();
      }
      if (appliedChannel) {
        sendBeacon();
      }
    }

    slot = (slot + 1) % DRONE_TX_SLOTS_PER_SECOND;
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(DRONE_TX_SLOT_MS));
  }
}

void taskDroneTransmitter() {
  xTaskCreatePinnedToCore(TaskDroneTransmitter, "TaskDroneTransmitter",
                          8192,  // NimBLE and the Wi-Fi driver start here
                          NULL,
                          1,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif
