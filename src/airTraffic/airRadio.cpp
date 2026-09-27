#include "config.h"
#ifdef THR_AIR_TRAFFIC
#include <RadioLib.h>

#include "airTraffic/airRadio.h"
#include "lora/loraPins.h"

/* Never used - this board does not transmit - but begin() needs one. The
   lowest the SX1262 accepts, so a transmission nobody wrote could only ever
   be a whisper. */
#define AIR_TX_POWER_DBM -9
/* Bits of preamble the receiver waits for before it looks for the sync word.
   FLARM sends a single byte of it, so anything longer would miss FLARM. */
#define AIR_FSK_PREAMBLE_BITS 8
#define AIR_LORA_PREAMBLE_SYMBOLS 8

static SX1262 radio =
    new Module(LORA_PIN_CS, LORA_PIN_DIO1, LORA_PIN_RESET, LORA_PIN_BUSY);

static volatile boolean packetWaiting = false;
static volatile uint32_t packetMillis = 0;

static AirListen current = AIR_LISTEN_NONE;
/* what the chip was last initialised as: a LoRa setting and a GFSK one differ
   in modem, and two GFSK ones may differ in bit rate */
static boolean chipIsLora = false;
static float chipBitRateKbps = 0;
static uint32_t refused = 0;
static uint32_t slowestSwitchMillis = 0;

static void IRAM_ATTR onPacket() {
  packetWaiting = true;
  packetMillis = millis();
}

/* What begin() does not keep: the reset it starts with forgets the antenna
   switch, the gain and the interrupt. See lora/loraPins.h for the switch. */
static void afterBegin() {
  radio.setDio2AsRfSwitch(true);
  radio.setRfSwitchPins(LORA_PIN_RF_SW, RADIOLIB_NC);
  /* a receiver that never transmits has no reason to save the few mA the
     boosted LNA costs */
  radio.setRxBoostedGainMode(true);
  radio.setPacketReceivedAction(onPacket);
}

static int16_t beginFsk(const AirListenSetting* setting) {
  radio.clearPacketReceivedAction();
  int16_t state = radio.beginFSK(setting->frequencyMhz, setting->bitRateKbps,
                                 setting->deviationKhz, setting->rxBandwidthKhz,
                                 AIR_TX_POWER_DBM, AIR_FSK_PREAMBLE_BITS,
                                 LORA_TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }
  afterBegin();
  /* ADS-L's Gaussian BT (C.2); only a transmitter uses it, and it is here so
     the chip is set to the protocol rather than to RadioLib's default */
  radio.setDataShaping(RADIOLIB_SHAPING_0_5);
  /* the CRCs of these protocols are nothing the chip can compute - FLARM's
     covers bytes that precede the payload, ADS-L's is 24 bits - and they sit
     under a Manchester coding the chip does not have either */
  radio.setCRC(0);
  radio.setWhitening(false);
  chipIsLora = false;
  chipBitRateKbps = setting->bitRateKbps;
  return RADIOLIB_ERR_NONE;
}

static int16_t beginLora(const AirListenSetting* setting) {
  radio.clearPacketReceivedAction();
  int16_t state = radio.begin(setting->frequencyMhz, setting->loraBandwidthKhz,
                              setting->spreadingFactor, setting->codingRate,
                              setting->loraSyncWord, AIR_TX_POWER_DBM,
                              AIR_LORA_PREAMBLE_SYMBOLS, LORA_TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }
  afterBegin();
  chipIsLora = true;
  chipBitRateKbps = 0;
  return RADIOLIB_ERR_NONE;
}

static int16_t applyFsk(const AirListenSetting* setting) {
  if (chipIsLora || chipBitRateKbps != setting->bitRateKbps) {
    int16_t state = beginFsk(setting);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
  } else {
    radio.standby();
    int16_t state = radio.setFrequency(setting->frequencyMhz);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
  }
  /* RadioLib takes the sync word by a non-const pointer and only reads it */
  radio.setSyncWord((uint8_t*)setting->syncWord, setting->syncWordLength);
  return radio.fixedPacketLengthMode(setting->captureLength);
}

boolean airRadioListen(AirListen listen) {
  const AirListenSetting* setting = airListenSetting(listen);
  if (setting == NULL) {
    radio.standby();
    current = AIR_LISTEN_NONE;
    return true;
  }
  uint32_t started = millis();
  /* The task reads what is waiting before it switches, so anything flagged
     now landed in between: it belongs to the setting being left, and reading
     it back afterwards would report it as heard on the new one. */
  packetWaiting = false;
  int16_t state = setting->lora ? beginLora(setting) : applyFsk(setting);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("Air radio: cannot switch to "));
    Serial.print(setting->name);
    Serial.print(F(", error "));
    Serial.println(state);
    current = AIR_LISTEN_NONE;
    return false;
  }
  radio.startReceive();
  current = listen;
  uint32_t took = millis() - started;
  if (took > slowestSwitchMillis) {
    slowestSwitchMillis = took;
  }
  return true;
}

boolean airRadioBegin() { return airRadioListen(AIR_LISTEN_MBAND_LOW); }

AirListen airRadioListening() { return current; }

boolean airRadioTake(AirCapture* capture) {
  if (!packetWaiting || current == AIR_LISTEN_NONE) {
    return false;
  }
  packetWaiting = false;
  capture->listen = current;
  capture->atMillis = packetMillis;
  capture->loraCrcChecked = false;

  size_t length = radio.getPacketLength();
  if (length > AIR_RAW_MAX) {
    length = AIR_RAW_MAX;
  }
  int16_t state = radio.readData(capture->raw, length);
  capture->length = (uint16_t)length;
  capture->rssi = (int16_t)radio.getRSSI();
  if (chipIsLora) {
    uint8_t codingRate = 0;
    bool hasCrc = false;
    if (radio.getLoRaRxHeaderInfo(&codingRate, &hasCrc) == RADIOLIB_ERR_NONE) {
      capture->loraCrcChecked = hasCrc;
    }
  }
  radio.startReceive();

  if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    refused++;
    return false;
  }
  return state == RADIOLIB_ERR_NONE && length > 0;
}

uint32_t airRadioRefused() { return refused; }

uint32_t airRadioSlowestSwitchMillis() { return slowestSwitchMillis; }
#endif
