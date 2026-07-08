#include "config.h"
#if BOARD_TYPE == KIND_LORA_GPS
#include <RadioLib.h>

#include "params.h"

// Direct peer-to-peer LoRa beacon (no LoRaWAN): every drone periodically
// broadcasts its own GPS fix and listens for the fixes of the other drones.

#define BEACON_FREQUENCY 868.0
#define BEACON_MAGIC 0xB7
// each emission happens after a random delay in [MIN, MIN + SPAN] ms so that
// drones spread out over time and collisions stay rare
#define BEACON_TX_MIN_MS 4000
#define BEACON_TX_SPAN_MS 4000
#define MAX_PEERS 8
// set to 0 to silence the per-packet TX/RX logging once the link is validated
#define BEACON_DEBUG 1

// unique per board (derived from the MAC) so each drone has a distinct id and
// does not mistake a peer's packet for its own echo
static uint16_t droneId = 0;

// SX1262 pins for the Seeed XIAO ESP32S3 LoRa module: CS, DIO1, RESET, BUSY
SX1262 radio = new Module(41, 39, 42, 40);

struct __attribute__((packed)) BeaconPacket {
  uint8_t magic;
  uint16_t droneId;
  int32_t latitude;   // degrees * 1e6
  int32_t longitude;  // degrees * 1e6
  int16_t altitude;   // meters
  uint8_t satellites;
};

struct Peer {
  uint16_t droneId;
  int32_t latitude;
  int32_t longitude;
  int16_t altitude;
  uint8_t satellites;
  uint32_t lastSeenMillis;
};

static Peer peers[MAX_PEERS];
static uint8_t peerCount = 0;
static volatile bool receivedFlag = false;

static void IRAM_ATTR setReceivedFlag() {
  receivedFlag = true;
}

static void storePeer(const BeaconPacket& packet) {
  uint8_t index = peerCount;
  for (uint8_t i = 0; i < peerCount; i++) {
    if (peers[i].droneId == packet.droneId) {
      index = i;
      break;
    }
  }
  if (index == peerCount) {
    if (peerCount >= MAX_PEERS) return;
    peerCount++;
  }
  peers[index].droneId = packet.droneId;
  peers[index].latitude = packet.latitude;
  peers[index].longitude = packet.longitude;
  peers[index].altitude = packet.altitude;
  peers[index].satellites = packet.satellites;
  peers[index].lastSeenMillis = millis();
}

static void handleReceived() {
  uint8_t buffer[sizeof(BeaconPacket)];
  size_t length = radio.getPacketLength();
  int state = radio.readData(buffer, sizeof(BeaconPacket));

#if BEACON_DEBUG
  Serial.print(F("RX "));
  Serial.print(length);
  Serial.print(F(" bytes rssi="));
  Serial.print(radio.getRSSI());
  Serial.print(F("dBm snr="));
  Serial.print(radio.getSNR());
  Serial.print(F("dB -> "));
#endif

  if (state != RADIOLIB_ERR_NONE) {
#if BEACON_DEBUG
    Serial.print(F("read error "));
    Serial.println(state);
#endif
    return;
  }

  BeaconPacket packet;
  memcpy(&packet, buffer, sizeof(BeaconPacket));
  if (length < sizeof(BeaconPacket) || packet.magic != BEACON_MAGIC) {
#if BEACON_DEBUG
    Serial.println(F("not a beacon"));
#endif
    return;
  }
  if (packet.droneId == droneId) {
#if BEACON_DEBUG
    Serial.println(F("own echo"));
#endif
    return;
  }

  storePeer(packet);

  Serial.print(F("Peer "));
  Serial.print(packet.droneId);
  Serial.print(F(": "));
  Serial.print(packet.latitude / 1e6, 6);
  Serial.print(F(", "));
  Serial.print(packet.longitude / 1e6, 6);
  Serial.print(F(" | alt="));
  Serial.print(packet.altitude);
  Serial.print(F("m | sats="));
  Serial.print(packet.satellites);
  Serial.print(F(" | rssi="));
  Serial.print(radio.getRSSI());
  Serial.println(F("dBm"));
}

static void transmitFix() {
  BeaconPacket packet;
  packet.magic = BEACON_MAGIC;
  packet.droneId = droneId;
  packet.latitude =
      getParameterInt32(PARAM_GPS_LATITUDE_LOW, PARAM_GPS_LATITUDE_HIGH);
  packet.longitude =
      getParameterInt32(PARAM_GPS_LONGITUDE_LOW, PARAM_GPS_LONGITUDE_HIGH);
  packet.altitude = getParameter(PARAM_GPS_ALTITUDE);
  packet.satellites = getParameter(PARAM_GPS_SATELLITES);

  // Listen-before-talk: only transmit if no other drone is currently on air.
  // Channel Activity Detection is fast and keeps two drones from stepping on
  // each other; back off a random time and retry if the channel is busy.
  for (uint8_t attempt = 0; attempt < 4; attempt++) {
    if (radio.scanChannel() == RADIOLIB_CHANNEL_FREE) break;
    vTaskDelay(20 + (esp_random() % 40));
  }

  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  radio.startReceive();
  receivedFlag = false;

#if BEACON_DEBUG
  Serial.print(F("TX beacon id="));
  Serial.print(droneId);
  Serial.print(F(" lat="));
  Serial.print(packet.latitude / 1e6, 6);
  Serial.print(F(" lon="));
  Serial.print(packet.longitude / 1e6, 6);
  Serial.print(F(" sats="));
  Serial.print(packet.satellites);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F(" (transmit error "));
    Serial.print(state);
    Serial.print(F(")"));
  }
  Serial.println();
#endif
}

void TaskLoraBeacon(void* pvParameters) {
  (void)pvParameters;
  vTaskDelay(5000);

  droneId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
  Serial.print(F("LoRa beacon droneId="));
  Serial.println(droneId);

  int state = radio.begin(BEACON_FREQUENCY, 125.0, 9, 7,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 22, 8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("LoRa beacon radio init failed: "));
    Serial.println(state);
    while (true) vTaskDelay(1000);
  }
  radio.setDio2AsRfSwitch(true);
  radio.setPacketReceivedAction(setReceivedFlag);
  radio.startReceive();

  uint32_t lastTransmit = 0;
  // fully randomized interval, re-rolled after every emission
  uint32_t nextInterval = BEACON_TX_MIN_MS + (esp_random() % BEACON_TX_SPAN_MS);

  while (true) {
    if (receivedFlag) {
      receivedFlag = false;
      handleReceived();
      radio.startReceive();
    }

    if (millis() - lastTransmit > nextInterval) {
      transmitFix();
      lastTransmit = millis();
      nextInterval = BEACON_TX_MIN_MS + (esp_random() % BEACON_TX_SPAN_MS);
    }

    vTaskDelay(10);
  }
}

void processLoraCommand(char command, char* paramValue, Print* output) {
  (void)paramValue;
  switch (command) {
    case 'i':
      output->print(F("Beacon droneId: "));
      output->println(droneId);
      output->print(F("Known peers: "));
      output->println(peerCount);
      for (uint8_t i = 0; i < peerCount; i++) {
        output->print(peers[i].droneId);
        output->print(F(": "));
        output->print(peers[i].latitude / 1e6, 6);
        output->print(F(", "));
        output->print(peers[i].longitude / 1e6, 6);
        output->print(F(" | alt="));
        output->print(peers[i].altitude);
        output->print(F("m | sats="));
        output->print(peers[i].satellites);
        output->print(F(" | age="));
        output->print((millis() - peers[i].lastSeenMillis) / 1000);
        output->println(F("s"));
      }
      break;
    default:
      output->println(F("(li) beacon info - droneId and known peers"));
      break;
  }
}

void taskLoraBeacon() {
  xTaskCreatePinnedToCore(TaskLoraBeacon, "TaskLoraBeacon",
                          8192,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif
