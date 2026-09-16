#include "config.h"
#ifdef THR_DRONE_ID
#include "droneId/droneIdBle.h"
#include "droneId/droneIdFeed.h"
#include "droneId/droneIdQueue.h"
#include "droneId/droneIdReport.h"
#include "droneId/droneIdTable.h"
#include "droneId/droneIdWifi.h"
#include "params.h"
#include "toHex.h"

/* The drone watcher: two radios, one console.

   ASTM F3411 - the same standard as ASD-STAN prEN 4709-002, which is what a
   European drone is built to - makes an aircraft broadcast who it is, where it
   is and where its operator is standing, in the clear, so that anybody
   underneath can read it. It is broadcast four ways, and this board listens to
   all four: Bluetooth 4 legacy advertising, Bluetooth 5 Long Range, a vendor
   element in a Wi-Fi beacon, and a Wi-Fi NAN service discovery frame.

   Both radios are one radio. The ESP32-S3 has a single 2.4 GHz front end, and
   Espressif's own coexistence table rates Wi-Fi promiscuous receive alongside
   Bluetooth as supported but unstable - a sniffer is not one of the four Wi-Fi
   states the arbiter reserves time for, so with Wi-Fi idle "the RF module is
   controlled by Bluetooth". Rather than let that be decided implicitly, the
   two take turns: (A) seconds of Bluetooth then (B) seconds of Wi-Fi, with the
   scan stopped and the receiver closed at each handover so the slice is one
   the arbiter cannot take back.

   Seven and three is where it starts, and the arithmetic is the aircraft's
   rather than ours: a position is transmitted at least once a second on every
   transport, so three seconds of Wi-Fi is three chances at a beacon and five
   or six NAN discovery windows, and seven of Bluetooth is seven advertisements
   from each of the two Bluetooth sets. Missing one is not missing an aircraft.
   Setting either to 0 gives the whole radio to the other. */

static boolean wifiWindow = false;
static uint32_t windowStartedMillis = 0;
static uint32_t lastSweepMillis = 0;

static uint32_t windowMillis(byte number, int16_t fallback) {
  int16_t seconds = getParameter(number);
  if (seconds == ERROR_VALUE || seconds < 0) {
    seconds = fallback;
  }
  return (uint32_t)seconds * 1000ul;
}

/* An untouched NVS key reads 0, not ERROR_VALUE, so unset cannot be recognised
   slot by slot: 0 is a legitimate (A) and a legitimate (B), and a board that
   came up on those two would listen to neither radio and be indistinguishable
   from a quiet sky. (E) is what says the block was never written, because a
   board that forgets an aircraft after no seconds at all is not a choice
   anybody made. Same shape as the mesh, which uses its radio triple. */
static void writeDefaultsWhenUntouched() {
  if (getQualifier() == DRONE_QUALIFIER) {
    return;
  }
  setAndSaveParameter(PARAM_DRONE_BLE_SECONDS, DRONE_BLE_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_WIFI_SECONDS, DRONE_WIFI_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_WIFI_CHANNEL, DRONE_WIFI_CHANNEL_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_LOG_SECONDS, DRONE_LOG_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_FORGET_SECONDS, DRONE_FORGET_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_FEED_SECONDS, DRONE_FEED_SECONDS_DEFAULT);
  setAndSaveParameter(PARAM_DRONE_PILOT_METRES, DRONE_PILOT_METRES_DEFAULT);
  setQualifier(DRONE_QUALIFIER);
}

static void updateWindows() {
  uint32_t bluetooth =
      windowMillis(PARAM_DRONE_BLE_SECONDS, DRONE_BLE_SECONDS_DEFAULT);
  uint32_t wifi =
      windowMillis(PARAM_DRONE_WIFI_SECONDS, DRONE_WIFI_SECONDS_DEFAULT);

  if (bluetooth > 0 && wifi > 0) {
    uint32_t length = wifiWindow ? wifi : bluetooth;
    if (millis() - windowStartedMillis >= length) {
      wifiWindow = !wifiWindow;
      windowStartedMillis = millis();
    }
  }
  /* With one of them off there is nothing to take turns with, so the other
     runs continuously and the window never flips. */
  droneIdBleListen(bluetooth > 0 && (wifi == 0 || !wifiWindow));
  droneIdWifiListen(wifi > 0 && (bluetooth == 0 || wifiWindow));
}

static void printDroneInfo(Print* output) {
  output->println(F("=== Drone Remote ID ==="));
  output->print(F("Bluetooth: "));
  output->print(droneIdBleScanning() ? F("listening") : F("idle"));
  output->print(F(", "));
  output->print(droneIdBleAdvertisements());
  output->println(F(" advertisements heard"));
  output->print(F("Wi-Fi: "));
  if (!droneIdWifiReady()) {
    output->println(F("did not start"));
  } else {
    output->print(droneIdWifiListening() ? F("listening on channel ")
                                         : F("idle, channel "));
    output->print(droneIdWifiChannel());
    output->print(F(", "));
    output->print(droneIdWifiFrames());
    output->println(F(" frames heard"));
  }
  /* Said rather than left to be discovered: an aircraft using the other social
     channel is legal, conformant and completely inaudible here. */
  output->println(F("5 GHz beacons and NAN are not heard - 2.4 GHz radio"));

  output->print(F("Drones in the table: "));
  output->print(droneIdTableCount());
  output->print(F(" of "));
  output->println(DRONE_MAX_AIRCRAFT);
  output->print(F("Frames refused: "));
  output->print(droneIdTableRejected());
  output->print(F(", dropped unread: "));
  output->print(droneIdQueueDropped());
  output->print(F(", split too long: "));
  output->println(droneIdBleTruncated());
  output->print(F("Rows evicted: "));
  output->println(droneIdTableEvicted());

  /* Which of the two things this board is: a console somebody reads, or a feed
     a database reads. On a bridge the blocks and the position lines are not
     printed at all, so an operator who does not know that is looking at a
     silent port and concluding the radios are dead. */
  output->print(F("Feed: "));
  if (droneIdFeedEnabled()) {
    output->print(F("JSON, every "));
    output->print(getParameter(PARAM_DRONE_FEED_SECONDS));
    output->println(F(" s - the console blocks are not printed"));
  } else {
    output->println(F("console (DA2 makes this board a bridge)"));
  }
}

/* What a payload actually looked like, which is the only way to tell a
   transmitter this decoder does not understand from one that is not there. */
static void printLastPayload(Print* output, boolean rejected) {
  static uint8_t payload[DRONE_MAX_PAYLOAD];
  uint8_t length = droneIdTableCopyLastPayload(payload, rejected);
  output->print(rejected ? F("Last refused: ") : F("Last accepted: "));
  if (length == 0) {
    output->println(F("none"));
    return;
  }
  output->print(length);
  output->print(F(" bytes, "));
  toHex(output, payload, length);
  output->println();
}

void processDroneCommand(char command, char* paramValue, Print* output) {
  switch (command) {
    case 'i':
      printDroneInfo(output);
      break;
    case 'l':
      droneIdReportList(output);
      break;
    case 'd':
      droneIdReportDetail(output, (uint8_t)atoi(paramValue));
      break;
    case 'h':
      printLastPayload(output, false);
      printLastPayload(output, true);
      break;
    case 'c':
      droneIdTableReset();
      setParameter(PARAM_DRONE_COUNT, 0);
      output->println(F("Drone list cleared"));
      break;
    default:
      output->println(F("(di) info - radios, table, refused frames"));
      output->println(F("(dl) list the drones heard"));
      output->println(F("(dd) everything one of them said, dd0"));
      output->println(
          F("(dh) the last payload accepted, and the last refused"));
      output->println(F("(dc) clear the drone list"));
      printParameterHelp(output, PARAM_DRONE_BLE_SECONDS,
                         F("seconds per cycle on Bluetooth, 0 = never"));
      printParameterHelp(output, PARAM_DRONE_WIFI_SECONDS,
                         F("seconds per cycle on Wi-Fi, 0 = never"));
      printParameterHelp(output, PARAM_DRONE_WIFI_CHANNEL,
                         F("Wi-Fi channel, 0 = hop over 6, 1, 6, 11"));
      printParameterHelp(output, PARAM_DRONE_LOG_SECONDS,
                         F("quiet seconds between two lines on one drone"));
      printParameterHelp(output, PARAM_DRONE_FORGET_SECONDS,
                         F("seconds of silence before a drone is dropped"));
      printParameterHelp(output, PARAM_DRONE_COUNT,
                         F("drones currently in the table"));
      printParameterHelp(
          output, PARAM_DRONE_PILOT_METRES,
          F("metres the operator moves before the feed says so"));
      printParameterHelp(output, PARAM_DRONE_FEED_SECONDS,
                         F("seconds between JSON feed lines, on a bridge"));
      break;
  }
}

void TaskDroneId(void* pvParameters) {
  (void)pvParameters;

  writeDefaultsWhenUntouched();
  droneIdQueueBegin();
  droneIdTableBegin();
  setParameter(PARAM_DRONE_COUNT, 0);
  droneIdBleBegin();
  droneIdWifiBegin();
  windowStartedMillis = millis();

  Serial.println(F("Drone Remote ID started. 'dl' lists what is flying."));

  DroneCapture capture;
  while (true) {
    updateWindows();

    /* Drained to empty before sleeping: a captured frame is a position, and a
       position printed late is a position that was true somewhere else. */
    while (droneIdQueuePop(&capture)) {
      droneIdTableApply(&Serial, &capture);
    }

    if (millis() - lastSweepMillis >= 1000) {
      lastSweepMillis = millis();
      droneIdTableExpire(&Serial);
      setParameter(PARAM_DRONE_COUNT, droneIdTableCount());
    }

    vTaskDelay(20);
  }
}

void taskDroneId() {
  xTaskCreatePinnedToCore(TaskDroneId, "TaskDroneId",
                          6144,  // the decoding and the console printing both
                                 // happen here, and printing a double is what
                                 // costs the stack
                          NULL,
                          1,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif
