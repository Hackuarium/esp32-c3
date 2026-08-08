#include "config.h"
#ifdef THR_LORA_MESH
#include <RadioLib.h>
#include <string.h>

#include "lora/loraBridge.h"
#include "lora/loraMesh.h"
#include "lora/loraPeers.h"
#include "params.h"
#include "toHex.h"

/* SX1262 pins of the Seeed XIAO ESP32S3 LoRa module: CS, DIO1, RESET, BUSY */
#ifndef LORA_PIN_CS
#define LORA_PIN_CS 41
#define LORA_PIN_DIO1 39
#define LORA_PIN_RESET 42
#define LORA_PIN_BUSY 40
#endif
/* The antenna hangs off a PE4259, and that switch takes two control lines:
   DIO2 drives CTRL and this pin drives /CTRL, so they have to move together and
   opposite. Leaving it floating does not fail, it attenuates - the receive path
   still happens to be selected, so a node hears normally and its own transmit
   only reaches the antenna through the switch's isolation, about 40 dB down.
   That is invisible on a bench at arm's length and costs a factor of a hundred
   in range outdoors. */
#ifndef LORA_PIN_RF_SW
#define LORA_PIN_RF_SW 38
#endif

/* Carrier as a count of 25 kHz steps above 400 MHz, overridable per env, so
   18736 is 868.4 MHz - the gap between the mandatory LoRaWAN channels at 868.3
   and 868.5, which is what makes it worth having: sub-band M is only 1% and
   14 dBm, but nothing else transmits there, and a channel to itself is worth
   more to this mesh than the sub-band P allowance it could be sharing with
   every LoRaWAN gateway's RX2 downlink. 18781 (869.525) is that alternative:
   500 mW, 10%, and the one 250 kHz channel the regulation grants - reach for it
   when a link needs the 8 dB rather than the quiet. */
#ifndef LORA_FREQUENCY_DEFAULT
#define LORA_FREQUENCY_DEFAULT 18736
#endif
/* Legacy DC values counted 0.1 MHz and covered 1500 to 9600. No band this radio
   uses lands there in the new encoding - 433.05-434.79 is 1322 to 1391 and
   863-870 is 18520 to 18800 - so a stored value in that window is unambiguously
   an old one, and is refused rather than believed. */
#define LORA_FREQUENCY_LEGACY_MIN 1500
#define LORA_FREQUENCY_LEGACY_MAX 9600
/* 125 kHz is the widest channel that fits between 868.3 and 868.5 without
   overlapping either, so outside sub-band P the carrier decides the bandwidth
   as much as the other way round. */
#ifndef LORA_BANDWIDTH_DEFAULT
#define LORA_BANDWIDTH_DEFAULT 125
#endif
/* SF9 is where this mesh's two scarce resources meet: 1% of an hour is 36 s of
   airtime, and a 29-byte telemetry frame costs 226 ms at SF9/125 kHz against
   1647 ms at SF12 - so SF12 would leave a tracker 21 frames an hour. The 7.5 dB
   given up against SF12 is roughly half the range in open ground, which is the
   price of a cadence worth transmitting at all. */
#ifndef LORA_SPREADING_FACTOR_DEFAULT
#define LORA_SPREADING_FACTOR_DEFAULT 9
#endif
#define LORA_CODING_RATE 5
#define LORA_PREAMBLE_SYMBOLS 8
/* The Wio-SX1262 clocks the radio from an active TCXO supplied by DIO3, and it
   is a 1.8 V part. RadioLib's begin() defaults this argument to 1.6 V - the
   lowest step the SX1262 regulator offers - so leaving it out runs the
   oscillator below spec: it still starts, so begin() reports no error, but the
   PLL locks to a marginal reference and the transmitted chirp is spectrally
   smeared. The receiver's correlator then throws most of the energy away, which
   reads as a weak *and* noisy packet - a fixed ~50 dB below budget with the SNR
   pinned near 5 dB however close the nodes are. It has to be stated per board,
   not guessed: a module with a plain crystal needs 0 here instead. */
#ifndef LORA_TCXO_VOLTAGE
#define LORA_TCXO_VOLTAGE 1.8f
#endif
/* RadioLib's begin() trips the PA over-current protection at 60 mA, which is
   the SX1261 default; an SX1262 draws about 118 mA at the 22 dBm sub-band P
   allows, so the ceiling clips the top of the band a node moves to when it
   needs range. 140 mA is what the SX1262 resets this register to on its own. */
#define LORA_PA_CURRENT_LIMIT_MA 140.0f
/* 25 mW ERP, which is 14 dBm, is the limit across the 863-870 SRD band.
   Sub-band P (869.4-869.65) allows 500 mW - more than the SX1262 can produce -
   so there the radio's own 22 dBm ceiling is what binds. The 433 MHz band is
   the other way round: it allows only 10 mW, so 14 dBm would be two and a half
   times over. The old flat 22 was inherited from the beacon code and was
   roughly six times over the limit at 868 MHz. */
#define LORA_TX_POWER_ERP_LIMITED 14
#define LORA_TX_POWER_HIGH_POWER_BAND 22
#define LORA_TX_POWER_433_BAND 10

/* EN 300 220 expresses the duty cycle as transmit time within an observation
   window, not as a gap between frames, so the governor is a budget rather than
   a delay: a node may burst until the window's allowance is spent, then waits
   for it to trickle back. One hour is the window the standard uses; shortening
   it here only makes the node more conservative. */
#ifndef LORA_DUTY_CYCLE_WINDOW_MS
#define LORA_DUTY_CYCLE_WINDOW_MS 3600000ul
#endif

/* how many other nodes have to be heard relaying a frame before this node gives
   up its own copy - a cheap approximation of Trickle suppression, and what
   keeps a flood from costing N^2 transmissions */
#define LORA_OVERHEARD_ENOUGH 2
#define LORA_RELAY_QUEUE_SIZE 4

/* How many counter values are claimed in NVS at a time. The stored value is a
   promise that nothing above it was ever used, so this is the flash wear knob:
   one write per N transmissions, paid for by burning N counters on every boot
   whether or not they were spent. */
#ifndef LORA_COUNTER_RESERVATION
#define LORA_COUNTER_RESERVATION 100
#endif

static SX1262 radio =
    new Module(LORA_PIN_CS, LORA_PIN_DIO1, LORA_PIN_RESET, LORA_PIN_BUSY);

static uint8_t groupKey[LORA_KEY_SIZE];
static boolean groupKeyPresent = false;
static uint8_t nodeAddress = 0;

static uint32_t transmitCounter = 0;
/* the highest counter already promised to flash: the stored value is always an
   upper bound, so a power loss can only ever move the counter forward, and a
   CCM nonce is never reused */
static uint32_t counterLimit = 0;

static volatile boolean receivedFlag = false;
/* transmit time still available in the current observation window, and the
   moment it was last credited */
static uint32_t airtimeBudgetMillis = 0;
static uint32_t budgetUpdatedMillis = 0;
static int16_t appliedSpreadingFactor = 0;
static float appliedFrequency = 0;
static float appliedBandwidth = 0;

struct RelayEntry {
  boolean active;
  uint8_t frame[LORA_MAX_FRAME_SIZE];
  uint8_t frameLength;
  uint8_t source;
  uint32_t counter;
  uint32_t dueMillis;
  uint8_t overheard;
};

static RelayEntry relayQueue[LORA_RELAY_QUEUE_SIZE];

struct PendingRequest {
  boolean active;
  uint8_t frame[LORA_MAX_FRAME_SIZE];
  uint8_t frameLength;
  uint8_t destination;
  uint32_t counter;
  uint8_t attempt;
  uint32_t dueMillis;
  Print* output;
};

static PendingRequest pending;

/* the radio, the peer table and the two queues are shared between TaskLoraMesh
   and the serial task. Transmitting from a serial command while the task is in
   readData wedges the SX1262 driver, so every access goes through this mutex.
   It is recursive because the task already holds it when it calls the same
   send helpers the serial menu uses */
static StaticSemaphore_t xMutexBufferLoraMesh;
static SemaphoreHandle_t xSemaphoreLoraMesh = NULL;

/* a transmission plus its listen-before-talk backoff can hold the mutex for a
   while, so a serial command has to be ready to wait for its turn */
static boolean takeLoraMesh(Print* output) {
  if (xSemaphoreLoraMesh == NULL) {
    output->println(F("LoRa mesh not started"));
    return false;
  }
  if (xSemaphoreTakeRecursive(xSemaphoreLoraMesh, pdMS_TO_TICKS(15000)) !=
      pdTRUE) {
    output->println(F("LoRa mesh busy, try again"));
    return false;
  }
  return true;
}

static void giveLoraMesh() {
  xSemaphoreGiveRecursive(xSemaphoreLoraMesh);
}

static void IRAM_ATTR setReceivedFlag() {
  receivedFlag = true;
}

static uint8_t spreadingFactor() {
  int16_t value = getParameter(PARAM_LORA_SPREADING_FACTOR);
  if (value < 7 || value > 12) {
    return LORA_SPREADING_FACTOR_DEFAULT;
  }
  return (uint8_t)value;
}

/* The carrier as a count of 25 kHz steps above 400 MHz. 25 kHz is the channel
   raster of sub-band P and divides every EU868 and US915 channel, so a step
   that fine is what makes 869.525 - the centre of the one 250 kHz channel the
   regulation grants 500 mW - expressible at all; 0.1 MHz could only reach
   869.5, a quarter channel low. Counting from 400 MHz keeps the whole SX1262
   tuning range, 150 to 960 MHz, inside a plain positive-and-negative int16, so
   no parameter needs an unsigned accessor.

   The SX1262 tunes 150 to 960 MHz, so anything outside that is an unset or
   stale parameter rather than a choice. Two windows inside it are rejected as
   well. 0 is what an untouched NVS key reads and would otherwise mean
   400.000 MHz, which is in no SRD band; 1500 to 9600 is what firmware before
   2026-08 wrote here, when the slot counted 0.1 MHz, and a node that was never
   reset would come up on 617.1 MHz rather than off the air. Nothing is written
   back - a stale value stays where it is and the node simply runs on the
   default until someone sets DC. */
#define LORA_FREQUENCY_CODE_MIN (-10000)  // 150 MHz
#define LORA_FREQUENCY_CODE_MAX 22400     // 960 MHz
static int16_t frequencyCode() {
  int16_t value = getParameter(PARAM_LORA_FREQUENCY);
  if (value == 0 || value < LORA_FREQUENCY_CODE_MIN ||
      value > LORA_FREQUENCY_CODE_MAX ||
      (value >= LORA_FREQUENCY_LEGACY_MIN &&
       value <= LORA_FREQUENCY_LEGACY_MAX)) {
    return LORA_FREQUENCY_DEFAULT;
  }
  return value;
}

static float frequency() {
  return 400.0f + frequencyCode() * 0.025f;
}

static float bandwidth() {
  switch (getParameter(PARAM_LORA_BANDWIDTH)) {
    case 250:
      return 250.0f;
    case 62:
    case 63:
      return 62.5f;
    case 125:
      return 125.0f;
    default:
      return (float)LORA_BANDWIDTH_DEFAULT;
  }
}

/* The share of the observation window this node may occupy, as one part in N.
   EN 300 220 splits 863-870 MHz into sub-bands with different duty cycles, so
   the legal limit follows the carrier and cannot be a constant. Anything not
   recognised falls back to the strictest value rather than the most convenient
   one. RadioLib does not enforce any of this outside LoRaWAN. */
static uint16_t dutyCycleDivisor() {
  int16_t carrier = frequencyCode();
  if (carrier >= 1322 && carrier <= 1391) {
    return 10;  // 433.05-434.79 MHz, 10%
  }
  if (carrier >= 18600 && carrier < 18744) {
    return 100;  // 865.0-868.6 MHz, sub-bands L and M, 1%
  }
  if (carrier >= 18776 && carrier <= 18786) {
    return 10;  // 869.4-869.65 MHz, sub-band P, 10%
  }
  if (carrier >= 18788 && carrier <= 18800) {
    return 100;  // 869.7-870.0 MHz, sub-band Q, 1%
  }
  return 1000;  // sub-bands K and N, and every gap, 0.1%
}

/* transmit time allowed per observation window, 36 s per hour at 1% */
static uint32_t dutyCycleAllowanceMillis() {
  return LORA_DUTY_CYCLE_WINDOW_MS / dutyCycleDivisor();
}

/* The power ceiling follows the carrier for the same reason the duty cycle
   does. It is deliberately not a parameter: there is no legitimate reason to
   set it higher, and a parameter is one typo away from transmitting illegally. */
static int8_t maxTxPowerDbm() {
  int16_t carrier = frequencyCode();
  if (carrier >= 1322 && carrier <= 1391) {
    return LORA_TX_POWER_433_BAND;  // 433.05-434.79 MHz, 10 mW
  }
  if (carrier >= 18776 && carrier <= 18786) {
    return LORA_TX_POWER_HIGH_POWER_BAND;  // sub-band P, 500 mW
  }
  return LORA_TX_POWER_ERP_LIMITED;
}

static uint8_t defaultTtl() {
  int16_t value = getParameter(PARAM_LORA_TTL);
  if (value < 0 || value > LORA_TTL_MAX) {
    return 2;
  }
  return (uint8_t)value;
}

static boolean isRepeater() {
  return getParameter(PARAM_LORA_ROLE) == LORA_ROLE_REPEATER;
}

/* seconds between two automatic HELLOs, 0 = never. A board that joined the mesh
   without resetting its parameters has this slot unset, and silence is not what
   an unset parameter should mean here, so anything negative is the default */
static uint32_t helloIntervalMillis() {
  int16_t value = getParameter(PARAM_LORA_HELLO_SECONDS);
  if (value < 0) {
    return LORA_HELLO_SECONDS_DEFAULT * 1000ul;
  }
  return (uint32_t)value * 1000ul;
}

/* Semtech's airtime formula for an explicit header at CR 4/5. Everything that
   paces this protocol - relay jitter, ACK timeouts, the duty cycle governor -
   is expressed as a multiple of it, because airtime, not byte count, is the
   scarce resource */
static uint32_t airtimeMillis(uint8_t frameLength) {
  uint8_t sf = spreadingFactor();
  double symbolMillis = (double)(1ul << sf) / bandwidth();
  /* the low data rate optimisation kicks in above a 16 ms symbol, which at
     125 kHz means SF11 but at 62.5 kHz already means SF10 - so it has to be
     derived from the symbol time, not from the spreading factor alone */
  uint8_t lowDataRateOptimize = symbolMillis > 16.0 ? 1 : 0;
  double numerator =
      8.0 * frameLength - 4.0 * sf + 28 + 16;
  double denominator = 4.0 * (sf - 2 * lowDataRateOptimize);
  double symbols = ceil(numerator / denominator) * (LORA_CODING_RATE);
  if (symbols < 0) {
    symbols = 0;
  }
  double total = (LORA_PREAMBLE_SYMBOLS + 4.25) * symbolMillis +
                 (8 + symbols) * symbolMillis;
  return (uint32_t)(total + 1);
}

/* Credits the airtime the window has given back since the last check. The
   leftover milliseconds stay on the clock instead of being rounded away, so a
   long run of small refills does not drift. Unsigned arithmetic makes the
   millis() wrap at 49 days a non-event. */
static void refillAirtimeBudget() {
  uint32_t now = millis();
  uint32_t gained = (now - budgetUpdatedMillis) / dutyCycleDivisor();
  if (gained > 0) {
    airtimeBudgetMillis += gained;
    budgetUpdatedMillis += gained * dutyCycleDivisor();
  }
  uint32_t allowance = dutyCycleAllowanceMillis();
  if (airtimeBudgetMillis > allowance) {
    airtimeBudgetMillis = allowance;
    budgetUpdatedMillis = now;
  }
}

static boolean canTransmit(uint8_t frameLength) {
  refillAirtimeBudget();
  return airtimeBudgetMillis >= airtimeMillis(frameLength);
}

static void printRadioSettings(Print* output) {
  output->print(F("Radio: "));
  output->print(frequency(), 3);
  output->print(F(" MHz, BW "));
  output->print(bandwidth(), 1);
  output->print(F(" kHz, SF"));
  output->print(spreadingFactor());
  output->print(F(", duty cycle 1/"));
  output->print(dutyCycleDivisor());
  output->print(F(", "));
  output->print(maxTxPowerDbm());
  output->println(F(" dBm"));
}

static void applyRadioSettings(Print* output) {
  appliedSpreadingFactor = spreadingFactor();
  appliedFrequency = frequency();
  appliedBandwidth = bandwidth();

  int state = radio.setFrequency(appliedFrequency);
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setBandwidth(appliedBandwidth);
  }
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setSpreadingFactor(appliedSpreadingFactor);
  }
  /* the ceiling depends on the sub-band, so moving the carrier can change it */
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setOutputPower(maxTxPowerDbm());
  }
  /* setOutputPower reads the trip point, reconfigures the PA and writes the old
     value back, so raising it has to follow every power change rather than
     happen once at startup */
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setCurrentLimit(LORA_PA_CURRENT_LIMIT_MA);
  }
  if (state != RADIOLIB_ERR_NONE) {
    if (output != NULL) {
      output->print(F("Radio settings rejected: "));
      output->println(state);
    }
    return;
  }
  if (output != NULL) {
    printRadioSettings(output);
  }
}

/* Listen before talk, then transmit and spend the airtime. */
static boolean transmitFrame(const uint8_t* frame, uint8_t frameLength) {
  if (!canTransmit(frameLength)) {
    return false;
  }
  for (uint8_t attempt = 0; attempt < 4; attempt++) {
    if (radio.scanChannel() == RADIOLIB_CHANNEL_FREE) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 40)));
  }

  int state = radio.transmit((uint8_t*)frame, frameLength);
  uint32_t airtime = airtimeMillis(frameLength);
  airtimeBudgetMillis =
      airtimeBudgetMillis > airtime ? airtimeBudgetMillis - airtime : 0;
  radio.startReceive();
  receivedFlag = false;

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("LoRa transmit failed: "));
    Serial.println(state);
    return false;
  }
  return true;
}

static void persistCounter() {
  counterLimit = transmitCounter + LORA_COUNTER_RESERVATION;
  setNVSParameterInt32("mesh.counter", (int32_t)counterLimit);
}

static uint32_t nextCounter() {
  transmitCounter++;
  if (transmitCounter >= counterLimit) {
    persistCounter();
  }
  return transmitCounter;
}

static uint8_t buildFrame(uint8_t destination,
                          uint8_t type,
                          uint8_t budget,
                          uint32_t counter,
                          const uint8_t* body,
                          uint8_t bodyLength,
                          uint8_t* buffer) {
  LoraFrame frame;
  frame.version = 0;
  frame.type = type;
  frame.budget = budget;
  frame.hops = 0;
  frame.routeLength = 0;
  frame.source = nodeAddress;
  frame.destination = destination;
  frame.counter = counter;
  frame.bodyLength = bodyLength;
  if (bodyLength > 0) {
    memcpy(frame.body, body, bodyLength);
  }
  return loraFrameEncode(&frame, groupKey, buffer, LORA_MAX_FRAME_SIZE);
}

static boolean expectsAcknowledge(uint8_t type) {
  return type == LORA_TYPE_CMD || type == LORA_TYPE_DATA_ACK;
}

/* The ladder: try direct, then ask for two relayed hops, then four. A retry
   always reuses the same counter, so the receiver can tell "the ACK was lost"
   from "he sent it again" - and because the budget sits in the trailer, outside
   the authenticated data, escalating rewrites one byte instead of re-encrypting.

   Three attempts, one per rung, rather than doubling up the first two. The
   ladder holds the single confirmed-request slot for its whole length, so its
   duration is what every other addressed command waits for: at SF9 the doubled
   version ran 10.5 s for a short frame and 18 s for a full one, which was long
   enough that a poller on a ten second timer refused nearly everything else the
   mesh had to say. Losing the duplicates costs a retransmission on a single
   dropped ACK; keeping all three rungs costs nothing, because the reach of the
   mesh is the top one. */
#define LORA_LADDER_ATTEMPTS 3

static uint8_t ladderTtl(uint8_t attempt) {
  if (attempt < 1) {
    return 0;
  }
  if (attempt < 2) {
    return 2;
  }
  return 4;
}

static uint32_t ladderTimeout(uint8_t attempt, uint8_t frameLength) {
  uint32_t airtime = airtimeMillis(frameLength);
  uint8_t hops = ladderTtl(attempt);
  return 2ul * (2ul * hops + 1ul) * airtime + 200ul;
}

static void sendAcknowledge(uint8_t destination,
                            uint8_t type,
                            uint32_t requestCounter,
                            uint8_t status,
                            uint8_t budget) {
  uint8_t body[4];
  body[0] = (uint8_t)(requestCounter >> 16);
  body[1] = (uint8_t)(requestCounter >> 8);
  body[2] = (uint8_t)requestCounter;
  body[3] = status;
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length =
      buildFrame(destination, type, budget, nextCounter(), body, 4, buffer);
  if (length > 0) {
    transmitFrame(buffer, length);
  }
}

/* The traffic events a bridge feeds to its host. They are written straight to
   Serial rather than to the caller's stream, so an automatic send reaches the
   feed even though it reports its progress nowhere. */
static void reportTransmission(uint8_t type,
                               uint8_t destination,
                               uint32_t counter,
                               boolean acknowledged) {
  Print* json = loraBridgeBegin("tx");
  loraBridgeText(json, "type", loraTypeName(type));
  loraBridgeInt(json, "dst", destination);
  loraBridgeInt(json, "counter", counter);
  loraBridgeInt(json, "ack", acknowledged ? 1 : 0);
  loraBridgeEnd(json);
}

/* The frame exactly as it came off the radio, before anything has been trusted:
   a bridge logs this even with no key or a wrong one, so a capture survives a
   node that cannot read what it heard. */
static void reportRawReception(const uint8_t* buffer,
                               uint8_t length,
                               int16_t rssi,
                               int8_t snr) {
  Print* json = loraBridgeBegin("raw");
  loraBridgeInt(json, "length", length);
  loraBridgeInt(json, "rssi", rssi);
  loraBridgeInt(json, "snr", snr);
  if (json != NULL) {
    json->print(F(",\"frame\":\""));
    toHex(json, buffer, length);
    json->print('"');
  }
  loraBridgeEnd(json);
}

static void reportReception(const LoraFrame* frame,
                            int16_t rssi,
                            int8_t snr,
                            boolean fresh) {
  Print* json = loraBridgeBegin("rx");
  loraBridgeText(json, "type", loraTypeName(frame->type));
  loraBridgeInt(json, "src", frame->source);
  loraBridgeInt(json, "dst", frame->destination);
  loraBridgeInt(json, "counter", frame->counter);
  loraBridgeInt(json, "budget", frame->budget);
  loraBridgeInt(json, "hops", frame->hops);
  loraBridgeInt(json, "rssi", rssi);
  loraBridgeInt(json, "snr", snr);
  loraBridgeInt(json, "fresh", fresh ? 1 : 0);
  if (json != NULL) {
    /* the path the frame took, each relay with the margin it heard it at. The
       last hop is this node, whose rssi is the reading above. */
    json->print(F(",\"route\":["));
    for (uint8_t i = 0; i < frame->routeLength; i++) {
      if (i > 0) {
        json->print(',');
      }
      json->print(F("{\"address\":"));
      json->print(frame->route[i].address);
      json->print(F(",\"rssi\":"));
      json->print(frame->route[i].rssi);
      json->print('}');
    }
    json->print(']');
    /* the plaintext body, so the host can keep what it cannot yet interpret */
    json->print(F(",\"body\":\""));
    toHex(json, frame->body, frame->bodyLength);
    json->print('"');
  }
  loraBridgeEnd(json);
}

void loraMeshResetAirtimeBudget() {
  airtimeBudgetMillis = dutyCycleAllowanceMillis();
  budgetUpdatedMillis = millis();
}

boolean loraMeshSend(uint8_t destination,
                     uint8_t type,
                     const uint8_t* body,
                     uint8_t bodyLength,
                     Print* output) {
  if (!groupKeyPresent) {
    output->println(F("No AES128 key set, use (ak)"));
    return false;
  }
  if (nodeAddress == 0 || nodeAddress == LORA_ADDRESS_BROADCAST) {
    output->println(F("No node address set, use (an)"));
    return false;
  }

  boolean acknowledged =
      expectsAcknowledge(type) && destination != LORA_ADDRESS_BROADCAST;

  if (!takeLoraMesh(output)) {
    return false;
  }

  if (acknowledged && pending.active) {
    output->println(F("A confirmed request is already in flight"));
    giveLoraMesh();
    return false;
  }

  uint32_t counter = nextCounter();
  /* a beacon exists to prove a direct link: relaying it would prove nothing and
     cost the whole mesh an airtime slot */
  uint8_t budget = acknowledged || type == LORA_TYPE_HELLO ? ladderTtl(0)
                                                           : defaultTtl();
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length =
      buildFrame(destination, type, budget, counter, body, bodyLength, buffer);
  if (length == 0) {
    output->println(F("Body too long"));
    giveLoraMesh();
    return false;
  }

  if (!acknowledged) {
    /* a broadcast is never acknowledged - 255 nodes answering one frame is an
       ACK storm, so a confirmed broadcast is polled node by node instead */
    boolean sent = transmitFrame(buffer, length);
    giveLoraMesh();
    if (!sent) {
      output->println(F("Duty cycle: cannot transmit yet"));
      return false;
    }
    output->print(F("Sent "));
    output->print(loraTypeName(type));
    output->print(F(" counter "));
    output->println(counter);
    reportTransmission(type, destination, counter, false);
    return true;
  }

  memcpy(pending.frame, buffer, length);
  pending.frameLength = length;
  pending.destination = destination;
  pending.counter = counter;
  pending.attempt = 0;
  pending.output = output;
  pending.active = true;

  /* skip the direct attempt when the peer is not a fresh neighbour */
  LoraPeer* peer = loraPeerFind(destination);
  if (peer == NULL || millis() - peer->lastHeardMillis > 30ul * 60ul * 1000ul) {
    pending.attempt = 1;
    loraFrameSetBudget(pending.frame, pending.frameLength,
                      ladderTtl(pending.attempt));
  }

  transmitFrame(pending.frame, pending.frameLength);
  pending.dueMillis =
      millis() + ladderTimeout(pending.attempt, pending.frameLength);
  pending.attempt++;
  giveLoraMesh();
  output->print(F("Sent "));
  output->print(loraTypeName(type));
  output->print(F(" counter "));
  output->print(counter);
  output->println(F(", waiting for ACK"));
  reportTransmission(type, destination, counter, true);
  return true;
}

static void queueRelay(const uint8_t* buffer,
                       uint8_t length,
                       uint8_t source,
                       uint32_t counter,
                       int16_t rssi) {
  RelayEntry* entry = NULL;
  for (uint8_t i = 0; i < LORA_RELAY_QUEUE_SIZE; i++) {
    if (!relayQueue[i].active) {
      entry = &relayQueue[i];
      break;
    }
  }
  if (entry == NULL) {
    return;
  }

  memcpy(entry->frame, buffer, length);
  entry->frameLength = length;
  /* the hop is counted and, while the table has room, signed with this node's
     address and the margin it was heard at */
  if (!loraFrameAppendRelay(entry->frame, &entry->frameLength,
                            LORA_MAX_FRAME_SIZE, nodeAddress, rssi)) {
    return;
  }
  entry->source = source;
  entry->counter = counter;
  entry->overheard = 0;
  entry->active = true;
  /* without this jitter every relay in range answers at the same instant and
     they all collide */
  entry->dueMillis = millis() + (esp_random() % (3 * airtimeMillis(length) + 1));
}

static void serviceRelayQueue() {
  for (uint8_t i = 0; i < LORA_RELAY_QUEUE_SIZE; i++) {
    RelayEntry* entry = &relayQueue[i];
    if (!entry->active || (int32_t)(millis() - entry->dueMillis) < 0) {
      continue;
    }
    if (entry->overheard >= LORA_OVERHEARD_ENOUGH) {
      entry->active = false;
      continue;
    }
    if (!canTransmit(entry->frameLength)) {
      /* the frame is already on its way through other paths; holding it until
         the budget refills would deliver it long after it is useful */
      entry->active = false;
      continue;
    }
    transmitFrame(entry->frame, entry->frameLength);
    entry->active = false;
  }
}

static void noteOverheard(uint8_t source, uint32_t counter) {
  for (uint8_t i = 0; i < LORA_RELAY_QUEUE_SIZE; i++) {
    RelayEntry* entry = &relayQueue[i];
    if (entry->active && entry->source == source &&
        entry->counter == counter) {
      entry->overheard++;
    }
  }
}

static void handleAcknowledge(const LoraFrame* frame) {
  if (frame->bodyLength < 4 || !pending.active) {
    return;
  }
  uint32_t acknowledged = ((uint32_t)frame->body[0] << 16) |
                          ((uint32_t)frame->body[1] << 8) |
                          (uint32_t)frame->body[2];
  if ((pending.counter & 0xFFFFFFul) != acknowledged) {
    return;
  }

  pending.active = false;
  Print* output = pending.output == NULL ? &Serial : pending.output;
  output->print(frame->type == LORA_TYPE_ACK ? F("ACK from ") : F("NACK from "));
  output->print(frame->source);
  output->print(F(" status "));
  output->println(frame->body[3]);
}

/* answers a read with the values themselves rather than a receipt. The request
   counter is echoed back so the caller can close its pending request, exactly
   as an ACK would */
static void sendParameterResponse(uint8_t destination,
                                  uint32_t requestCounter,
                                  uint8_t firstParameter,
                                  uint8_t count,
                                  uint8_t budget) {
  uint8_t body[LORA_MAX_BODY_SIZE];
  body[0] = (uint8_t)(requestCounter >> 16);
  body[1] = (uint8_t)(requestCounter >> 8);
  body[2] = (uint8_t)requestCounter;
  uint8_t encoded = loraMeshEncodeLocalParameters(
      firstParameter, count, body + LORA_RESP_COUNTER_SIZE,
      LORA_MAX_BODY_SIZE - LORA_RESP_COUNTER_SIZE);
  if (encoded == 0) {
    sendAcknowledge(destination, LORA_TYPE_NACK, requestCounter,
                    LORA_REASON_OUT_OF_RANGE, budget);
    return;
  }

  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length =
      buildFrame(destination, LORA_TYPE_RESP, budget, nextCounter(), body,
                 LORA_RESP_COUNTER_SIZE + encoded, buffer);
  if (length > 0) {
    transmitFrame(buffer, length);
  }
}

/* the console answer, sent once the queued command has actually run: same shape
   as a parameter response, but the payload is already built by the time it gets
   here because running the command is what produced it */
static void sendConsoleResponse(const LoraConsoleReply* reply) {
  uint8_t body[LORA_MAX_BODY_SIZE];
  body[0] = (uint8_t)(reply->requestCounter >> 16);
  body[1] = (uint8_t)(reply->requestCounter >> 8);
  body[2] = (uint8_t)reply->requestCounter;
  memcpy(body + LORA_RESP_COUNTER_SIZE, reply->body, reply->bodyLength);

  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length = buildFrame(
      reply->destination, LORA_TYPE_RESP, reply->budget, nextCounter(), body,
      (uint8_t)(LORA_RESP_COUNTER_SIZE + reply->bodyLength), buffer);
  if (length > 0) {
    transmitFrame(buffer, length);
  }
}

/* Runs whatever a remote (ar) asked for, outside the receive path. It is a slow
   step by design - the command is a whole console command, and a few of them
   block for seconds - so it sits with the other services in the task loop
   rather than inside the radio callback. */
static void serviceConsole() {
  LoraConsoleReply reply;
  if (!loraMeshRunQueuedConsole(&reply)) {
    return;
  }
  sendConsoleResponse(&reply);
}

static void handleResponse(const LoraFrame* frame) {
  if (frame->bodyLength < LORA_RESP_COUNTER_SIZE + 1) {
    return;
  }
  uint32_t echoed = ((uint32_t)frame->body[0] << 16) |
                    ((uint32_t)frame->body[1] << 8) | (uint32_t)frame->body[2];
  if (pending.active && (pending.counter & 0xFFFFFFul) == echoed) {
    pending.active = false;
  }

  const uint8_t* body = frame->body + LORA_RESP_COUNTER_SIZE;
  uint8_t bodyLength = (uint8_t)(frame->bodyLength - LORA_RESP_COUNTER_SIZE);
  if (body[0] == LORA_CMD_CONSOLE) {
    loraMeshReportConsole(frame->source, body, bodyLength);
    return;
  }
  if (bodyLength < 3) {
    return;
  }
  loraMeshReportParameters(frame->source, body, bodyLength);
}

static void handleCommand(const LoraFrame* frame, boolean duplicate) {
  LoraPeer* peer = loraPeerGet(frame->source);

  /* a read changes nothing, so a duplicate is simply answered again rather than
     suppressed - the retry exists because the answer was lost */
  if (frame->bodyLength >= 3 && frame->body[0] == LORA_CMD_GET_PARAMETERS) {
    if (frame->destination != LORA_ADDRESS_BROADCAST) {
      sendParameterResponse(frame->source, frame->counter, frame->body[1],
                            frame->body[2], frame->hops);
    }
    return;
  }

  if (duplicate) {
    /* the request got through but our ACK did not: answer again, execute once */
    if (peer->hasAcknowledged && peer->acknowledgedCounter == frame->counter) {
      sendAcknowledge(frame->source, LORA_TYPE_ACK, frame->counter,
                      peer->acknowledgedStatus, frame->hops);
    }
    return;
  }

  /* a console command is queued rather than applied: it runs from the task
     loop, after this ACK is on the air. A broadcast one is dropped outright -
     the answer is a frame addressed back to the caller, so every node would
     transmit its own console at once. */
  uint8_t status;
  if (frame->bodyLength >= 1 && frame->body[0] == LORA_CMD_CONSOLE) {
    status = frame->destination == LORA_ADDRESS_BROADCAST
                 ? LORA_REASON_UNKNOWN_COMMAND
                 : loraMeshQueueConsole(frame->source, frame->counter,
                                        frame->hops, frame->body,
                                        frame->bodyLength);
  } else {
    status = loraMeshApplyCommand(frame->body, frame->bodyLength);
  }
  peer->acknowledgedCounter = frame->counter;
  peer->acknowledgedStatus = status;
  peer->hasAcknowledged = true;

  Print* json = loraBridgeBegin("cmd");
  loraBridgeInt(json, "src", frame->source);
  loraBridgeInt(json, "status", status);
  loraBridgeEnd(json);
  if (json == NULL) {
    Serial.print(F("CMD from "));
    Serial.print(frame->source);
    Serial.print(F(" status "));
    Serial.println(status);
  }

  if (frame->destination != LORA_ADDRESS_BROADCAST) {
    sendAcknowledge(frame->source,
                    status == LORA_STATUS_OK ? LORA_TYPE_ACK : LORA_TYPE_NACK,
                    frame->counter, status, frame->hops);
  }
}

static void handleReceived() {
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint16_t length = radio.getPacketLength();
  if (length == 0 || length > LORA_MAX_FRAME_SIZE) {
    radio.readData(buffer, 0);
    return;
  }
  int state = radio.readData(buffer, length);
  if (state != RADIOLIB_ERR_NONE) {
    return;
  }

  int16_t rawRssi = (int16_t)radio.getRSSI();
  int8_t rawSnr = (int8_t)radio.getSNR();
  reportRawReception(buffer, (uint8_t)length, rawRssi, rawSnr);

  if (!groupKeyPresent) {
    return;
  }

  LoraFrame frame;
  /* the tag is verified before anything else looks at the frame: only authentic
     group traffic is ever acted on, and - just as important - only authentic
     traffic is ever amplified by a relay */
  if (!loraFrameDecode(buffer, (uint8_t)length, groupKey, &frame)) {
    /* the raw event above already carries the bytes; this says the tag did not
       verify, so the host can tell a foreign transmitter from a decode it lost */
    Print* json = loraBridgeBegin("reject");
    loraBridgeInt(json, "length", length);
    loraBridgeEnd(json);
    return;
  }
  if (frame.source == nodeAddress) {
    return;
  }

  /* count overheard copies before the duplicate test drops them, otherwise the
     suppression heuristic would never see anything */
  noteOverheard(frame.source, frame.counter);

  boolean fresh = loraPeerAcceptCounter(frame.source, frame.counter);
  LoraPeer* peer = loraPeerGet(frame.source);
  peer->lastRssi = rawRssi;
  peer->lastSnr = rawSnr;

  boolean forMe = frame.destination == nodeAddress ||
                  frame.destination == LORA_ADDRESS_BROADCAST;

  /* a bridge reports what it hears, not only what is addressed to it: the point
     of the role is to observe the mesh */
  reportReception(&frame, peer->lastRssi, peer->lastSnr, fresh);

  /* the budget counts up now: a frame travels while it has hops left, and
     "do not relay" is a budget of zero rather than an exhausted countdown */
  uint8_t remaining =
      frame.budget > frame.hops ? frame.budget - frame.hops : 0;
  if (fresh && isRepeater() && remaining > 0 &&
      frame.destination != nodeAddress) {
    if (remaining > LORA_TTL_MAX_ACCEPT) {
      Serial.print(F("Relay refused, remaining hops "));
      Serial.println(remaining);
    } else {
      queueRelay(buffer, (uint8_t)length, frame.source, frame.counter,
                 peer->lastRssi);
    }
  }

  if (!forMe) {
    return;
  }

  switch (frame.type) {
    case LORA_TYPE_ACK:
    case LORA_TYPE_NACK:
      if (fresh) {
        handleAcknowledge(&frame);
      }
      break;
    case LORA_TYPE_CMD:
      handleCommand(&frame, !fresh);
      break;
    case LORA_TYPE_RESP:
      if (fresh) {
        handleResponse(&frame);
      }
      break;
    case LORA_TYPE_DATA:
    case LORA_TYPE_DATA_ACK:
      if (fresh) {
        loraMeshReportData(frame.source, frame.body, frame.bodyLength);
      }
      if (frame.type == LORA_TYPE_DATA_ACK &&
          frame.destination != LORA_ADDRESS_BROADCAST) {
        sendAcknowledge(frame.source, LORA_TYPE_ACK, frame.counter,
                        LORA_STATUS_OK, frame.hops);
      }
      break;
    case LORA_TYPE_HELLO:
      /* the rx event already carries it, and a bridge's port stays parseable */
      if (fresh && !loraMeshIsBridge()) {
        Serial.print(F("HELLO from "));
        Serial.print(frame.source);
        Serial.print(F(" rssi "));
        Serial.println(peer->lastRssi);
      }
      break;
    default:
      break;
  }
}

static void servicePending() {
  if (!pending.active || (int32_t)(millis() - pending.dueMillis) < 0) {
    return;
  }
  if (pending.attempt >= LORA_LADDER_ATTEMPTS) {
    pending.active = false;
    Print* output = pending.output == NULL ? &Serial : pending.output;
    output->print(F("No ACK from "));
    output->println(pending.destination);
    Print* json = loraBridgeBegin("noack");
    loraBridgeInt(json, "dst", pending.destination);
    loraBridgeInt(json, "counter", pending.counter);
    loraBridgeEnd(json);
    return;
  }
  if (!canTransmit(pending.frameLength)) {
    pending.dueMillis = millis() + 500;
    return;
  }

  loraFrameSetBudget(pending.frame, pending.frameLength,
                      ladderTtl(pending.attempt));
  transmitFrame(pending.frame, pending.frameLength);
  pending.dueMillis =
      millis() + ladderTimeout(pending.attempt, pending.frameLength);
  pending.attempt++;
}

static void sendHello() {
  if (!groupKeyPresent || nodeAddress == 0) {
    return;
  }
  uint8_t buffer[LORA_MAX_FRAME_SIZE];
  uint8_t length =
      buildFrame(LORA_ADDRESS_BROADCAST, LORA_TYPE_HELLO, 0, nextCounter(),
                 NULL, 0, buffer);
  if (length > 0) {
    transmitFrame(buffer, length);
  }
}

static void loadIdentity() {
  int32_t stored = getNVSParameterInt32("mesh.address");
  nodeAddress = (stored > 0 && stored < LORA_ADDRESS_BROADCAST)
                    ? (uint8_t)stored
                    : 0;
  groupKeyPresent = getBlobParameter("mesh.key", groupKey, LORA_KEY_SIZE);
}

boolean loraMeshTake(Print* output) {
  return takeLoraMesh(output);
}

void loraMeshGive() {
  giveLoraMesh();
}

uint8_t loraMeshAddress() {
  return nodeAddress;
}

boolean loraMeshHasKey() {
  return groupKeyPresent;
}

void loraMeshReloadIdentity() {
  loadIdentity();
}

void TaskLoraMesh(void* pvParameters) {
  (void)pvParameters;
  vTaskDelay(5000);

  loadIdentity();

  int32_t storedCounter = getNVSParameterInt32("mesh.counter");
  transmitCounter = storedCounter > 0 ? (uint32_t)storedCounter : 0;
  /* start at the reserved bound rather than at whatever the last run reached,
     so a crash mid-block can never replay a counter: forward is always safe for
     receivers, backward reuses a nonce */
  persistCounter();

  int state = radio.begin(frequency(), bandwidth(), spreadingFactor(),
                          LORA_CODING_RATE,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE, maxTxPowerDbm(),
                          LORA_PREAMBLE_SYMBOLS, LORA_TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("LoRa mesh radio init failed: "));
    Serial.println(state);
    while (true) {
      vTaskDelay(1000);
    }
  }
  appliedSpreadingFactor = spreadingFactor();
  appliedFrequency = frequency();
  appliedBandwidth = bandwidth();
  /* the node was silent before it booted, so it starts owed the whole window */
  airtimeBudgetMillis = dutyCycleAllowanceMillis();
  budgetUpdatedMillis = millis();
  radio.setDio2AsRfSwitch(true);
  /* the other half of the switch: driven low while transmitting, high while
     listening, which is exactly the complement of DIO2 */
  radio.setRfSwitchPins(LORA_PIN_RF_SW, RADIOLIB_NC);
  radio.setCurrentLimit(LORA_PA_CURRENT_LIMIT_MA);
  /* the measured floor sits about 6 dB above what the datasheet claims for this
     part, and the boosted LNA is the cheapest of that back: a little more
     current while listening, which neither a mains-powered bridge nor a node
     that is idle between frames has any reason to save */
  radio.setRxBoostedGainMode(true);
  radio.setPacketReceivedAction(setReceivedFlag);
  radio.startReceive();

  Serial.print(F("LoRa mesh started, address "));
  Serial.print(nodeAddress);
  Serial.print(F(", counter "));
  Serial.print(transmitCounter);
  Serial.println(groupKeyPresent ? F(", key set") : F(", NO KEY"));

  /* the first HELLO goes out as soon as the task runs: hours are a long time
     for a node that has just booted to stay out of its neighbours' peer tables */
  boolean helloSent = false;
  uint32_t lastHello = 0;
  uint32_t lastBroadcast = 0;

  while (true) {
    xSemaphoreTakeRecursive(xSemaphoreLoraMesh, portMAX_DELAY);

    if (receivedFlag) {
      receivedFlag = false;
      handleReceived();
      radio.startReceive();
    }

    if (spreadingFactor() != appliedSpreadingFactor ||
        frequency() != appliedFrequency || bandwidth() != appliedBandwidth) {
      applyRadioSettings(&Serial);
      radio.startReceive();
    }

    serviceRelayQueue();
    servicePending();
    serviceConsole();

    uint32_t helloInterval = helloIntervalMillis();
    if (helloInterval > 0 &&
        (!helloSent || millis() - lastHello > helloInterval)) {
      helloSent = true;
      lastHello = millis();
      sendHello();
    }

    int16_t interval = getParameter(PARAM_LORA_INTERVAL_SECONDS);
    if (interval > 0 &&
        millis() - lastBroadcast > (uint32_t)interval * 1000ul) {
      lastBroadcast = millis();
      loraMeshBroadcastParameters();
    }

    giveLoraMesh();
    vTaskDelay(10);
  }
}

/* The peer table as one JSON object, the shape a host stores rather than the
   one a human reads. */
static void printPeersJson(Print* output) {
  output->print(F("{\"event\":\"peers\",\"count\":"));
  output->print(loraPeerCount());
  output->print(F(",\"peers\":["));
  for (uint8_t i = 0; i < loraPeerCount(); i++) {
    LoraPeer* peer = loraPeerAt(i);
    if (i > 0) {
      output->print(',');
    }
    output->print(F("{\"address\":"));
    output->print(peer->address);
    output->print(F(",\"counter\":"));
    output->print(peer->lastCounter);
    output->print(F(",\"rssi\":"));
    output->print(peer->lastRssi);
    output->print(F(",\"snr\":"));
    output->print(peer->lastSnr);
    output->print(F(",\"age\":"));
    output->print((millis() - peer->lastHeardMillis) / 1000);
    output->print('}');
  }
  output->println(F("]}"));
}

void loraMeshPrintPeers(Print* output) {
  if (loraMeshIsBridge()) {
    printPeersJson(output);
    /* the answer to a command that arrived over MQTT or the web page is echoed
       on the port, so the host sees exchanges it did not start */
    Print* copy = loraBridgeCopy(output);
    if (copy != NULL) {
      printPeersJson(copy);
    }
    return;
  }

  output->print(F("Peers: "));
  output->println(loraPeerCount());
  for (uint8_t i = 0; i < loraPeerCount(); i++) {
    LoraPeer* peer = loraPeerAt(i);
    output->print(peer->address);
    output->print(F(": counter "));
    output->print(peer->lastCounter);
    output->print(F(" rssi "));
    output->print(peer->lastRssi);
    output->print(F("dBm snr "));
    output->print(peer->lastSnr);
    output->print(F("dB age "));
    output->print((millis() - peer->lastHeardMillis) / 1000);
    output->println(F("s"));
  }
}

void loraMeshPrintInfo(Print* output) {
  output->println(F("LoRa mesh information"));
  output->print(F("Address: "));
  output->println(nodeAddress);
  output->print(F("Role: "));
  if (loraMeshIsBridge()) {
    output->println(F("bridge"));
  } else {
    output->println(isRepeater() ? F("repeater") : F("endpoint"));
  }
  output->print(F("AES128 key: "));
  if (groupKeyPresent) {
    toHex(output, groupKey, LORA_KEY_SIZE);
    output->println();
  } else {
    output->println(F("not set"));
  }
  output->print(F("Counter: "));
  output->print(transmitCounter);
  output->print(F(" (reserved through "));
  output->print(counterLimit);
  output->println(F(")"));
  output->print(F("Default TTL: "));
  output->println(defaultTtl());
  printRadioSettings(output);
  refillAirtimeBudget();
  output->print(F("Airtime budget: "));
  output->print(airtimeBudgetMillis / 1000.0, 1);
  output->print(F(" s of "));
  output->print(dutyCycleAllowanceMillis() / 1000);
  output->print(F(" s per "));
  output->print(LORA_DUTY_CYCLE_WINDOW_MS / 60000ul);
  output->println(F(" min"));
  loraMeshPrintPeers(output);
}

/* A board joins the mesh by adding two lines to its config, so it has to be
   able to join without a parameter reset too - and on a board that has one, a
   reset wipes everything else it was configured for. An untouched NVS key reads
   0, not ERROR_VALUE, which is why the block cannot be recognised as unset slot
   by slot: 0 is a legitimate DB (originate frames nobody relays) and a
   legitimate DI (never announce itself), and those are exactly the two a node
   would then be stuck with, silently absent from every peer table.

   The radio settings are the exception, since no carrier, no bandwidth and no
   spreading factor is not a configuration anyone chose. All three reading 0 is
   therefore the one unambiguous signature of a block that was never written,
   and it is the only case that is filled in. A stale or deliberate value is
   left alone: the accessors already fall back for each of them individually. */
static void applyDefaultsIfNeverWritten() {
  if (getParameter(PARAM_LORA_FREQUENCY) != 0 ||
      getParameter(PARAM_LORA_BANDWIDTH) != 0 ||
      getParameter(PARAM_LORA_SPREADING_FACTOR) != 0) {
    return;
  }
  Serial.println(F("LoRa mesh parameters unset, writing the defaults"));
  loraMeshResetParameters();
}

void taskLoraMesh() {
  applyDefaultsIfNeverWritten();
  xSemaphoreLoraMesh = xSemaphoreCreateRecursiveMutexStatic(&xMutexBufferLoraMesh);
  xTaskCreatePinnedToCore(TaskLoraMesh, "TaskLoraMesh",
                          8192,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif
