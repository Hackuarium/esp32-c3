#include "config.h"
#ifdef THR_BLE_BEACON
#include <NimBLEDevice.h>
#include <stdio.h>
#include <string.h>

#include "params.h"

#if !CONFIG_BT_NIMBLE_EXT_ADV
#error "The beacon needs -D CONFIG_BT_NIMBLE_EXT_ADV=1 in the env: without it \
NimBLE only builds legacy advertising, so the coded PHY - the whole reason this \
board reaches further than a phone does - cannot be asked for."
#endif

/* The transmitter, and the other half of the observer in taskBLE.cpp.

   It exists because the thing being hunted cannot be borrowed: a VespaFinder
   tag is glued to a hornet, and nothing else in the fleet advertises, so there
   was no way to exercise (bs), (bk) and the range model at all. This board is a
   tag that can be told what to be - as loud as the radio goes to find the far
   end of a field, as quiet as a real tag to check a calibration - and it says
   what it is on the air rather than in a note beside it.

   The three settings are parameters and the advertising set is rebuilt whenever
   one of them changes, because a beacon at the far end of a field is not
   somewhere you go to reboot. */

/* The controller's ladder, esp_power_level_t: 3 dB rungs from -27 to +18, where
   +18 is where this chip stops. */
#define BLE_BEACON_TX_POWER_MIN (-27)
#define BLE_BEACON_TX_POWER_MAX 18
#define BLE_BEACON_TX_POWER_STEP 3

/* 20 ms is the shortest an extended advertisement may repeat. The long end is
   this task's own: past ten seconds a beacon is missed far more often than it
   is heard, and looks like one that is out of range. */
#define BLE_BEACON_INTERVAL_MIN_MS 20
#define BLE_BEACON_INTERVAL_MAX_MS 10000

/* the advertising set this board owns - it only ever has one */
#define BLE_BEACON_INSTANCE 0

#define BLE_BEACON_NAME_LENGTH 16
static char beaconName[BLE_BEACON_NAME_LENGTH] = "beacon";

/* what the running advertisement was built from, so a change typed on the
   console is noticed without asking the controller what it is doing */
static int16_t appliedTxPower = ERROR_VALUE;
static int16_t appliedInterval = ERROR_VALUE;
static int16_t appliedPhy = ERROR_VALUE;

/* Rounded down to the rung below rather than to the nearest: every step of this
   ladder is 3 dB of somebody else's channel, and the top of it is a legal
   ceiling. */
static int8_t txPowerDbm() {
  int16_t value = getParameter(PARAM_BLE_TX_POWER);
  if (value == ERROR_VALUE) {
    value = BLE_BEACON_TX_POWER_DEFAULT;
  }
  if (value > BLE_BEACON_TX_POWER_MAX) {
    value = BLE_BEACON_TX_POWER_MAX;
  }
  if (value < BLE_BEACON_TX_POWER_MIN) {
    value = BLE_BEACON_TX_POWER_MIN;
  }
  int16_t rung = (value - BLE_BEACON_TX_POWER_MIN) / BLE_BEACON_TX_POWER_STEP;
  return (int8_t)(BLE_BEACON_TX_POWER_MIN + rung * BLE_BEACON_TX_POWER_STEP);
}

static esp_power_level_t txPowerLevel() {
  /* ESP_PWR_LVL_N27 is 0 and every rung above it is 3 dB, so the index is the
     distance up the ladder */
  return (esp_power_level_t)((txPowerDbm() - BLE_BEACON_TX_POWER_MIN) /
                             BLE_BEACON_TX_POWER_STEP);
}

static uint16_t intervalMillis() {
  int16_t value = getParameter(PARAM_BLE_ADV_INTERVAL);
  if (value == ERROR_VALUE) {
    return BLE_BEACON_INTERVAL_DEFAULT_MS;
  }
  if (value < BLE_BEACON_INTERVAL_MIN_MS) {
    return BLE_BEACON_INTERVAL_MIN_MS;
  }
  if (value > BLE_BEACON_INTERVAL_MAX_MS) {
    return BLE_BEACON_INTERVAL_MAX_MS;
  }
  return (uint16_t)value;
}

static uint8_t advertisingPhy() {
  return getParameter(PARAM_BLE_PHY) == BLE_HCI_LE_PHY_1M ? BLE_HCI_LE_PHY_1M
                                                          : BLE_HCI_LE_PHY_CODED;
}

/* ble_gap_ext_adv_params counts intervals in 0.625 ms units */
static uint32_t intervalUnits() {
  return (uint32_t)intervalMillis() * 8 / 5;
}

/* An unwritten NVS key reads 0, not ERROR_VALUE, so unset cannot be recognised
   slot by slot: 0 dBm is a legitimate power and 0 ms an interval that is merely
   too short. The PHY is what says the block was never written - 0 is not a PHY
   anyone chose - which is the same tell the mesh takes from its radio triple. */
static void writeDefaultsIfUnset() {
  int16_t phy = getParameter(PARAM_BLE_PHY);
  if (phy == BLE_HCI_LE_PHY_1M || phy == BLE_HCI_LE_PHY_CODED) {
    return;
  }
  setAndSaveParameter(PARAM_BLE_TX_POWER, BLE_BEACON_TX_POWER_DEFAULT);
  setAndSaveParameter(PARAM_BLE_ADV_INTERVAL, BLE_BEACON_INTERVAL_DEFAULT_MS);
  setAndSaveParameter(PARAM_BLE_PHY, BLE_BEACON_PHY_DEFAULT);
}

/* The last three bytes of the address, so a bench with several of these on it
   can tell them apart without reading the whole address off every line. */
static void buildBeaconName() {
  std::string address = NimBLEDevice::getAddress().toString();
  if (address.length() < 17) {
    return;
  }
  snprintf(beaconName, sizeof(beaconName), "beacon-%c%c%c%c%c%c", address[9],
           address[10], address[12], address[13], address[15], address[16]);
}

static void startAdvertising() {
  NimBLEExtAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->stop(BLE_BEACON_INSTANCE);

  uint8_t phy = advertisingPhy();
  int8_t dbm = txPowerDbm();
  NimBLEDevice::setPower(txPowerLevel(), ESP_BLE_PWR_TYPE_ADV);
  NimBLEDevice::setPower(txPowerLevel(), ESP_BLE_PWR_TYPE_DEFAULT);

  NimBLEExtAdvertisement advertisement(phy, phy);
  /* legacy PDUs exist only on the 1M PHY, and they are the whole reason to ask
     for it: an extended advertisement on 1M is no more visible to a phone than
     a coded one */
  advertisement.setLegacyAdvertising(phy == BLE_HCI_LE_PHY_1M);
  advertisement.setConnectable(false);
  advertisement.setScannable(false);
  advertisement.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advertisement.setName(beaconName);
  /* what the controller is asked to radiate, and separately the TX Power AD
     field so a receiver reads it off the air - it is what (bi) on an observer
     reports as advertised, and the only thing an unknown tag says about itself */
  advertisement.setTxPower(dbm);
  uint8_t txPowerField[] = {2, BLE_HS_ADV_TYPE_TX_PWR_LVL, (uint8_t)dbm};
  advertisement.addData(txPowerField, sizeof(txPowerField));
  advertisement.setMinInterval(intervalUnits());
  advertisement.setMaxInterval(intervalUnits());

  if (!advertising->setInstanceData(BLE_BEACON_INSTANCE, advertisement) ||
      !advertising->start(BLE_BEACON_INSTANCE)) {
    Serial.println(F("[beacon] the controller refused the advertising set"));
    return;
  }

  appliedTxPower = getParameter(PARAM_BLE_TX_POWER);
  appliedInterval = getParameter(PARAM_BLE_ADV_INTERVAL);
  appliedPhy = getParameter(PARAM_BLE_PHY);
}

static void printBeaconInfo(Print* output) {
  output->println(F("=== Bluetooth beacon ==="));
  output->print(F("Advertising: "));
  output->println(
      NimBLEDevice::getAdvertising()->isActive(BLE_BEACON_INSTANCE) ? F("yes")
                                                                   : F("no"));
  output->print(F("Name: "));
  output->println(beaconName);
  output->print(F("Address: "));
  output->println(NimBLEDevice::getAddress().toString().c_str());

  output->print(F("PHY: "));
  if (advertisingPhy() == BLE_HCI_LE_PHY_1M) {
    output->println(F("1M legacy - every scanner sees it, C3 for coded"));
  } else {
    output->println(F("coded - ~7 dB further, invisible to a phone (C1)"));
  }

  output->print(F("Power: "));
  output->print(txPowerDbm());
  output->print(F(" dBm"));
  int16_t asked = getParameter(PARAM_BLE_TX_POWER);
  if (asked != ERROR_VALUE && asked != txPowerDbm()) {
    output->print(F(" (asked for "));
    output->print(asked);
    output->print(F(", the ladder has 3 dB rungs and stops at "));
    output->print(BLE_BEACON_TX_POWER_MAX);
    output->print(')');
  }
  output->println();

  output->print(F("Interval: "));
  output->print(intervalMillis());
  output->println(F(" ms"));

  output->print(F("Watch it from a listener with: bs"));
  output->println(NimBLEDevice::getAddress().toString().c_str());
}

/* The three settings are parameters and nothing else - a verb that only wrote
   one of them would be a second spelling of the same slot, and the task picks
   up a change whoever made it. */
void processBleCommand(char command, char* paramValue, Print* output) {
  (void)paramValue;
  switch (command) {
    case 'i':
      printBeaconInfo(output);
      break;
    default:
      output->println(F("(bi) info - name, address, PHY, power, interval"));
      printParameterHelp(output, PARAM_BLE_TX_POWER,
                         F("dBm, -27 to 18 in 3 dB steps"));
      printParameterHelp(output, PARAM_BLE_ADV_INTERVAL,
                         F("ms between advertisements, 20 to 10000"));
      printParameterHelp(output, PARAM_BLE_PHY,
                         F("1 = 1M and legacy, 3 = coded and long range"));
      break;
  }
}

void TaskBLEBeacon(void* pvParameters) {
  (void)pvParameters;

  writeDefaultsIfUnset();

  NimBLEDevice::init("");
  buildBeaconName();
  startAdvertising();

  Serial.print(F("[beacon] "));
  Serial.print(beaconName);
  Serial.print(F(" at "));
  Serial.println(NimBLEDevice::getAddress().toString().c_str());

  while (true) {
    if (appliedTxPower != getParameter(PARAM_BLE_TX_POWER) ||
        appliedInterval != getParameter(PARAM_BLE_ADV_INTERVAL) ||
        appliedPhy != getParameter(PARAM_BLE_PHY)) {
      startAdvertising();
      /* the one line this board prints on its own: it is usually watched from
         the other end, where a setting that failed to apply looks exactly like
         a beacon that went out of range */
      Serial.print(F("[beacon] "));
      Serial.print(txPowerDbm());
      Serial.print(F(" dBm every "));
      Serial.print(intervalMillis());
      Serial.print(F(" ms on "));
      Serial.println(advertisingPhy() == BLE_HCI_LE_PHY_1M ? F("1M")
                                                           : F("coded"));
    } else if (!NimBLEDevice::getAdvertising()->isActive(BLE_BEACON_INSTANCE)) {
      /* the NimBLE host resets on its own errors and comes back idle, and a
         beacon left in a field has to notice that itself */
      startAdvertising();
    }

    vTaskDelay(1000);
  }
}

void taskBLEBeacon() {
  xTaskCreatePinnedToCore(TaskBLEBeacon, "TaskBLEBeacon",
                          4096,  // NimBLE is initialised from this task
                          NULL,
                          1,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif
