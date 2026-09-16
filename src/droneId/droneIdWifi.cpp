#include "config.h"
#ifdef THR_DRONE_ID
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <string.h>

#include "droneIdFrames.h"
#include "droneIdQueue.h"
#include "droneIdSurvey.h"
#include "droneIdWifi.h"
#include "params.h"

#define WIFI_SUBTYPE_BEACON 8
#define WIFI_SUBTYPE_ACTION 13
/* the 802.11 source address, Address 2 of the management header */
#define WIFI_SOURCE_ADDRESS_OFFSET 10

/* Channel 6 is the social channel: an aircraft transmitting there is allowed
   to do so once a second, and anywhere else it must transmit five times a
   second. So a receiver that parks on 6 hears the slow ones and a receiver
   that wanders hears the fast ones badly. NAN settles it - its discovery
   windows are 16 ms every 524 ms on channel 6, and time spent elsewhere is
   discovery windows missed - so 6 is where this sits by default, and the hop
   order gives it half the windows when hopping is asked for. */
static const uint8_t hopChannels[] = {6, 1, 6, 11};
#define WIFI_HOP_COUNT (sizeof(hopChannels) / sizeof(hopChannels[0]))

static boolean wifiReady = false;
static boolean wifiListening = false;
static uint8_t currentChannel = 0;
static uint8_t hopIndex = 0;
static uint32_t dwellStartedMillis = 0;
/* Every management frame handed to the sniffer. Beacons alone make this
   climb in any building, so a zero says the receiver is not running
   rather than that nothing flew past. */
static uint32_t framesHeard = 0;

/* Where the frame is turned into something the drone task can read. Locating
   the payload is droneIdFrames.cpp's job; this only decides which locator to
   ask, and copies. */
static void queueFrame(const uint8_t* frame, uint8_t source,
                       const uint8_t* payload, uint8_t payloadLength,
                       int8_t rssi, uint8_t channel) {
  DroneCapture capture;
  memcpy(capture.address, &frame[WIFI_SOURCE_ADDRESS_OFFSET],
         DRONE_ADDRESS_LENGTH);
  capture.source = source;
  capture.rssi = rssi;
  capture.channel = channel;
  capture.length = payloadLength;
  memcpy(capture.payload, payload, payloadLength);
  droneIdQueuePush(&capture);
}

/* Runs inside the Wi-Fi driver task. It filters, copies and returns: printing
   a hundred bytes here would be nine milliseconds at 115200 baud, which is
   frames lost, and calling any esp_wifi function from here re-enters the task
   that is running it. */
static void receiveFrame(void* buffer, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) {
    return;
  }
  framesHeard++;
  const wifi_promiscuous_pkt_t* packet = (const wifi_promiscuous_pkt_t*)buffer;
  /* sig_len counts the four-byte frame check sequence, which is not part of
     the frame being walked. Anything shorter than the 24-byte management
     header plus that has no source address to read; the two locators check
     their own further requirements. */
  if (packet->rx_ctrl.sig_len < 24 + 4) {
    return;
  }
  uint16_t length = (uint16_t)(packet->rx_ctrl.sig_len - 4);
  const uint8_t* frame = packet->payload;
  int8_t rssi = (int8_t)packet->rx_ctrl.rssi;
  uint8_t channel = (uint8_t)packet->rx_ctrl.channel;

  uint8_t subtype = (frame[0] >> 4) & 0x0F;
  uint8_t payloadLength = 0;
  const uint8_t* payload = NULL;
  uint8_t source = DRONE_SOURCE_WIFI_BEACON;
  if (subtype == WIFI_SUBTYPE_BEACON) {
    droneIdSurveyBeacon(frame, length);
    payload = droneIdFindBeaconPayload(frame, length, &payloadLength);
  } else if (subtype == WIFI_SUBTYPE_ACTION) {
    payload = droneIdFindNanPayload(frame, length, &payloadLength);
    source = DRONE_SOURCE_WIFI_NAN;
  }
  if (payload != NULL && payloadLength > 0) {
    queueFrame(frame, source, payload, payloadLength, rssi, channel);
  }
}

static boolean applyChannel(uint8_t channel) {
  if (channel == currentChannel) {
    return true;
  }
  if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    return false;
  }
  currentChannel = channel;
  return true;
}

void droneIdWifiBegin() {
  esp_netif_init();
  /* Already created when something else brought Wi-Fi up first; that is not an
     error and must not be treated as one. */
  esp_event_loop_create_default();

  wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
  /* A sniffer never transmits, never associates and only ever wants management
     frames, so the buffers that serve aggregation and transmission are given
     back. Aggregated frames are data frames; this filter never sees one. */
  config.static_rx_buf_num = 6;
  config.dynamic_rx_buf_num = 16;
  config.ampdu_rx_enable = 0;
  config.ampdu_tx_enable = 0;
  config.nvs_enable = 0;

  if (esp_wifi_init(&config) != ESP_OK) {
    Serial.println(F("[drone] Wi-Fi refused to start, Bluetooth only"));
    return;
  }
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  /* No station, no access point: nothing to associate with, and an associated
     station would pin the channel to its own. */
  esp_wifi_set_mode(WIFI_MODE_NULL);
  if (esp_wifi_start() != ESP_OK) {
    Serial.println(F("[drone] Wi-Fi refused to start, Bluetooth only"));
    return;
  }

  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(&receiveFrame);
  wifiReady = true;
}

/* The channel is chosen on a dwell timer of its own rather than on the window
   handover. Both matter: a beacon on channel 6 arrives once a second, so a
   dwell shorter than that catches nothing - and stepping on the handover alone
   would freeze the hop at its first channel the moment Bluetooth is switched
   off with (A0), because then the Wi-Fi window never ends. applyChannel's
   short-circuit still keeps esp_wifi_set_channel to one call per real change,
   which is the point: it is the call this ESP-IDF has a reported deadlock
   against when made rapidly from an application task. */
#ifndef DRONE_WIFI_DWELL_MS
#define DRONE_WIFI_DWELL_MS 2000
#endif

static void updateChannel() {
  int16_t requested = getParameter(PARAM_DRONE_WIFI_CHANNEL);
  if (requested >= 1 && requested <= 14) {
    applyChannel((uint8_t)requested);
    return;
  }
  if (currentChannel != 0 &&
      millis() - dwellStartedMillis < DRONE_WIFI_DWELL_MS) {
    return;
  }
  dwellStartedMillis = millis();
  applyChannel(hopChannels[hopIndex]);
  hopIndex = (uint8_t)((hopIndex + 1) % WIFI_HOP_COUNT);
}

void droneIdWifiListen(boolean listening) {
  if (!wifiReady) {
    return;
  }
  if (listening) {
    updateChannel();
  }
  if (listening != wifiListening &&
      esp_wifi_set_promiscuous(listening) == ESP_OK) {
    wifiListening = listening;
  }
}

boolean droneIdWifiListening() { return wifiListening; }

uint8_t droneIdWifiChannel() { return currentChannel; }

uint32_t droneIdWifiFrames() { return framesHeard; }

boolean droneIdWifiReady() { return wifiReady; }
#endif
