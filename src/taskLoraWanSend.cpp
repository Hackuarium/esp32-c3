#include "config.h"
#if BOARD_TYPE == KIND_LORAWAN
#include <ArduinoNvs.h>
#include <string.h>
#include "esp_sleep.h"
#include "params.h"
#include "taskLoraWanConfig.h"
#include "toHex.h"

#define BAND 868E6

void gotoSleep(int seconds);

void waitOrSleep();
int8_t getLoraVersion();
void saveLoraSession(Print* output);
void resetLoraSession(Print* output);
void startLoraSession(Print* output, boolean restore);
void applyLoraRadioSettings(Print* output);
void updateLoRaParameters();
uint8_t loraSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
int16_t loraState = 0;

/* the node, the key arrays and loraSession are shared between TaskLoraWanSend
   and the serial task: reinitialising the node from a serial command while an
   uplink is in the air wedges the SX1262 driver. Every access to them goes
   through this mutex */
static StaticSemaphore_t xMutexBufferLora;
static SemaphoreHandle_t xSemaphoreLora = NULL;

/* an uplink holds the node until the end of the Rx2 window, so a serial command
   has to be ready to wait a few seconds for its turn */
static boolean takeLoraNode(Print* output) {
  if (xSemaphoreLora == NULL) {
    output->println(F("LoRa not started"));
    return false;
  }
  if (xSemaphoreTake(xSemaphoreLora, pdMS_TO_TICKS(15000)) != pdTRUE) {
    output->println(F("LoRa node busy, try again"));
    return false;
  }
  return true;
}

/* beginABP always wipes the session (fCntUp goes back to 0), so a stored
   session can only be injected in the window between beginABP and activateABP:
   setBufferSession does nothing at all once the session is active, and
   activateABP is what promotes a restored session from PENDING to ACTIVE.
   Everything that starts the node therefore goes through this function. */
void startLoraSession(Print* output, boolean restore) {
  updateLoRaParameters();

  if (getLoraVersion() == 0) {
    output->println(F("LoRaWAN 1.0"));
    loraState = node.beginABP(devAddr, NULL, NULL, nwkSEncKey, appSKey);
  } else {
    output->println(F("LoRaWAN 1.1"));
    loraState =
        node.beginABP(devAddr, fNwkSIntKey, sNwkSIntKey, nwkSEncKey, appSKey);
  }
  if (loraState != RADIOLIB_ERR_NONE) {
    debug(true, F("Initialise node failed"), loraState, false);
    return;
  }

  if (restore) {
    if (getBlobParameter("lora.session", loraSession,
                         RADIOLIB_LORAWAN_SESSION_BUF_SIZE)) {
      int16_t restoreState = node.setBufferSession(loraSession);
      if (restoreState != RADIOLIB_ERR_NONE) {
        debug(true, F("Load session failed"), restoreState, false);
        /* a rejected buffer has already been copied into the node, so it must be
           wiped or its stale channel settings would be saved back */
        node.clearSession();
      }
    } else {
      output->println(F("No stored session in NVS"));
    }
  }

  /* activateABP reports its outcome with negative status codes that are not
     errors, so its return value must not be compared to RADIOLIB_ERR_NONE */
  int16_t activateState = node.activateABP();
  if (activateState == RADIOLIB_LORAWAN_SESSION_RESTORED) {
    output->print(F("Session restored, FCntUp: "));
    output->println(node.getFCntUp());
  } else if (activateState == RADIOLIB_LORAWAN_NEW_SESSION) {
    output->println(F("New session, FCntUp restarts at 0"));
  } else if (activateState != RADIOLIB_ERR_NONE) {
    debug(true, F("Activate ABP failed"), activateState, false);
    loraState = activateState;
    return;
  }

  loraState = RADIOLIB_ERR_NONE;
  applyLoraRadioSettings(output);
  saveLoraSession(output);
}

static int16_t appliedSpreadingFactor = 0;
/* the datarate of the last uplink, so the spreading factor ADR settled on can be
   reported as a number instead of just "ADR". -1 until the first uplink */
static int16_t lastUplinkDatarate = -1;

/* EU868 datarates 0 to 5 are SF12 down to SF7, the rest are BW250, FSK and
   LR-FHSS and have no single spreading factor */
static void printSpreadingFactor(Print* output) {
  output->print(F("Spreading factor: "));
  if (appliedSpreadingFactor >= 7 && appliedSpreadingFactor <= 12) {
    output->print(F("SF"));
    output->print(appliedSpreadingFactor);
    output->println(F(" (fixed by T)"));
    return;
  }
  if (lastUplinkDatarate >= 0 && lastUplinkDatarate <= 5) {
    output->print(F("SF"));
    output->print(12 - lastUplinkDatarate);
  } else if (lastUplinkDatarate < 0) {
    output->print(F("unknown, no uplink yet"));
  } else {
    output->print(F("datarate "));
    output->print(lastUplinkDatarate);
  }
  output->println(F(" (chosen by ADR, set T to 7-12 to fix it)"));
}

/* EU868 maps DR0..DR5 to SF12..SF7, so the datarate is 12 - SF. A fixed
   spreading factor only holds with ADR off, otherwise the network moves the
   device to whatever datarate it prefers */
void applyLoraRadioSettings(Print* output) {
  /* RadioLib ships with the duty cycle disabled, but the 1% of EU868 is a legal
     limit, not a preference: enabling it makes sendReceive refuse an uplink that
     comes too early rather than transmit it */
  node.setDutyCycle(true, 0);
  node.setDwellTime(true, 0);

  appliedSpreadingFactor = getParameter(PARAM_LORAWAN_SPREADING_FACTOR);
  if (appliedSpreadingFactor < 7 || appliedSpreadingFactor > 12) {
    node.setADR(true);
    printSpreadingFactor(output);
    return;
  }

  node.setADR(false);
  int16_t datarateState = node.setDatarate(12 - appliedSpreadingFactor);
  if (datarateState != RADIOLIB_ERR_NONE) {
    debug(true, F("Set spreading factor failed"), datarateState, false);
    return;
  }
  printSpreadingFactor(output);
}

void resetLoraSession(Print* output) {
  deleteParameter("lora.session");
  startLoraSession(output, false);
}

void saveLoraSession(Print* output) {
  if (loraState != RADIOLIB_ERR_NONE) {
    output->println(F("Cannot save LoRaWAN session because of error"));
    return;
  }
  memcpy(loraSession, node.getBufferSession(),
         RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  NVS.setBlob("lora.session", loraSession, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
}

/* every multi-byte field of the session buffer sits at an odd offset, so it
   must be assembled byte by byte: a uint32_t* cast there is an unaligned read.
   RadioLib stores them little-endian (see LoRaWANNode::hton) */
static uint32_t readUint32(const uint8_t* buffer, uint16_t offset) {
  return (uint32_t)buffer[offset] | ((uint32_t)buffer[offset + 1] << 8) |
         ((uint32_t)buffer[offset + 2] << 16) |
         ((uint32_t)buffer[offset + 3] << 24);
}

static uint16_t readUint16(const uint8_t* buffer, uint16_t offset) {
  return (uint16_t)buffer[offset] | ((uint16_t)buffer[offset + 1] << 8);
}

static void printCounter(Print* output,
                         const __FlashStringHelper* name,
                         uint32_t value) {
  output->print(name);
  output->print(F(": "));
  if (value == RADIOLIB_LORAWAN_FCNT_NONE) {
    output->println(F("none"));
  } else {
    output->println(value);
  }
}

static void printSessionStatus(Print* output, uint8_t status) {
  output->print(F("Status: "));
  switch (status) {
    case RADIOLIB_LORAWAN_SESSION_NONE:
      output->println(F("none"));
      break;
    case RADIOLIB_LORAWAN_SESSION_ACTIVATING:
      output->println(F("activating"));
      break;
    case RADIOLIB_LORAWAN_SESSION_PENDING:
      output->println(F("pending (restored, not activated)"));
      break;
    case RADIOLIB_LORAWAN_SESSION_ACTIVE:
      output->println(F("active"));
      break;
    default:
      output->println(status);
      break;
  }
}

static void printKey(Print* output,
                     const __FlashStringHelper* name,
                     const uint8_t* key) {
  output->print(name);
  output->print(F(": "));
  toHex(output, (uint8_t*)key, RADIOLIB_AES128_KEY_SIZE);
  output->println();
}

/* dumps the buffer as RadioLib lays it out, so it can be used both for the
   live session and for the blob read back from NVS */
static void printSessionBuffer(Print* output, const uint8_t* buffer) {
  printSessionStatus(output, buffer[RADIOLIB_LORAWAN_SESSION_STATUS]);
  output->print(F("DevAddr: "));
  output->println(readUint32(buffer, RADIOLIB_LORAWAN_SESSION_DEV_ADDR), HEX);
  printKey(output, F("NwkSEncKey"),
           buffer + RADIOLIB_LORAWAN_SESSION_NWK_SENC_KEY);
  printKey(output, F("AppSKey"), buffer + RADIOLIB_LORAWAN_SESSION_APP_SKEY);
  printKey(output, F("FNwkSIntKey"),
           buffer + RADIOLIB_LORAWAN_SESSION_FNWK_SINT_KEY);
  printKey(output, F("SNwkSIntKey"),
           buffer + RADIOLIB_LORAWAN_SESSION_SNWK_SINT_KEY);
  printCounter(output, F("FCntUp (next)"),
               readUint32(buffer, RADIOLIB_LORAWAN_SESSION_FCNT_UP));
  printCounter(output, F("N_FCntDown"),
               readUint32(buffer, RADIOLIB_LORAWAN_SESSION_N_FCNT_DOWN));
  printCounter(output, F("A_FCntDown"),
               readUint32(buffer, RADIOLIB_LORAWAN_SESSION_A_FCNT_DOWN));
  printCounter(output, F("ADR_FCnt"),
               readUint32(buffer, RADIOLIB_LORAWAN_SESSION_ADR_FCNT));
  printCounter(output, F("ConfFCntUp"),
               readUint32(buffer, RADIOLIB_LORAWAN_SESSION_CONF_FCNT_UP));
  printCounter(output, F("ConfFCntDown"),
               readUint32(buffer, RADIOLIB_LORAWAN_SESSION_CONF_FCNT_DOWN));
  output->print(F("RJCount0: "));
  output->println(readUint16(buffer, RADIOLIB_LORAWAN_SESSION_RJ_COUNT0));
  output->print(F("RJCount1: "));
  output->println(readUint16(buffer, RADIOLIB_LORAWAN_SESSION_RJ_COUNT1));
  output->print(F("HomeNetID: "));
  output->println(readUint32(buffer, RADIOLIB_LORAWAN_SESSION_HOMENET_ID), HEX);
  output->print(F("Version: 1."));
  output->println(buffer[RADIOLIB_LORAWAN_SESSION_VERSION]);
}

void printLoraSession(Print* output) {
  output->println(F("--- live session ---"));
  output->print(F("Activated: "));
  output->println(node.isActivated() ? F("yes") : F("no"));
  output->print(F("DevAddr: "));
  output->println(node.getDevAddr(), HEX);
  output->print(F("FCntUp (last sent): "));
  output->println(node.getFCntUp());
  memcpy(loraSession, node.getBufferSession(),
         RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  printSessionBuffer(output, loraSession);

  output->println(F("--- saved in NVS ---"));
  if (getBlobParameter("lora.session", loraSession,
                       RADIOLIB_LORAWAN_SESSION_BUF_SIZE)) {
    printSessionBuffer(output, loraSession);
  } else {
    output->println(F("No stored session"));
  }
}

void TaskLoraWanSend(void* pvParameters) {
  vTaskDelay(5000);
  loraState = radio.begin();
  debug(loraState != RADIOLIB_ERR_NONE, F("Initialise radio failed"), loraState,
        true);

  xSemaphoreTake(xSemaphoreLora, portMAX_DELAY);
  startLoraSession(&Serial, true);
  xSemaphoreGive(xSemaphoreLora);

  // code from:
  // https://github.com/jgromes/RadioLib/blob/master/examples/LoRaWAN/LoRaWAN_ABP/LoRaWAN_ABP.ino

  while (true) {
    xSemaphoreTake(xSemaphoreLora, portMAX_DELAY);

    /* a failed activation must not wedge the task forever - the keys may have
       been set over serial since the last attempt */
    if (!node.isActivated()) {
      startLoraSession(&Serial, true);
    }

    if (node.isActivated()) {
      /* pick up a spreading factor changed over serial without needing a reset */
      if (getParameter(PARAM_LORAWAN_SPREADING_FACTOR) != appliedSpreadingFactor) {
        applyLoraRadioSettings(&Serial);
      }

      uint8_t uplinkPayload[12];
      for (int i = 0; i < 6; i++) {
        int16_t value = getParameter(i);
        uplinkPayload[i * 2] = lowByte(value);
        uplinkPayload[i * 2 + 1] = highByte(value);
      }
      LoRaWANEvent_t uplinkEvent;
      int16_t sendState = node.sendReceive(uplinkPayload, sizeof(uplinkPayload),
                                           1, false, &uplinkEvent);
      debug(sendState < RADIOLIB_ERR_NONE, F("Error in sendReceive"), sendState,
            false);
      /* the event is only filled once the uplink actually left the radio */
      if (sendState >= RADIOLIB_ERR_NONE) {
        lastUplinkDatarate = uplinkEvent.datarate;
      }

      /* the frame counter has advanced whether or not the uplink succeeded, so
         it must be persisted now: a reboot with a stale counter makes the
         network reject every uplink as a replay */
      saveLoraSession(&Serial);
    }

    xSemaphoreGive(xSemaphoreLora);
    waitOrSleep();
  }
}

void waitOrSleep() {
  if (getParameter(PARAM_LORAWAN_SLEEP_SECONDS) > 0) {
    // we should sleep if uptime is more than 300s. Too difficult to debug
    // otherwise

    if (millis() > 300 * 1000) {
      gotoSleep(getParameter(PARAM_LORAWAN_SLEEP_SECONDS));
    } else {
      vTaskDelay(getParameter(PARAM_LORAWAN_SLEEP_SECONDS) * 1000);
    }
  }

  /* This should allow the sensor to update the values */
  vTaskDelay(1000);

  // we wait for the next uplink interval
  // this is to avoid sending too often and to respect the FUP
  if (getParameter(PARAM_LORAWAN_SLEEP_SECONDS) +
          getParameter(PARAM_LORAWAN_INTERVAL_SECONDS) <
      10) {
    vTaskDelay(10 * 1000);
  } else if (getParameter(PARAM_LORAWAN_INTERVAL_SECONDS) > 0) {
    vTaskDelay(getParameter(PARAM_LORAWAN_INTERVAL_SECONDS) * 1000);
  }

  /* the configured interval can be shorter than the duty cycle allows, and it
     always is at the slow spreading factors - waiting here keeps the device
     legal instead of letting sendReceive reject the uplink */
  RadioLibTime_t untilUplink = node.timeUntilUplink();
  if (untilUplink > 0) {
    Serial.print(F("Duty cycle: waiting "));
    Serial.print(untilUplink / 1000);
    Serial.println(F("s"));
    vTaskDelay(pdMS_TO_TICKS(untilUplink));
  }
}

void printLoraWanHelp(Print* output) {
  output->println(F("(ai) info - print keys, version"));
  output->println(F("(ad) set DevAddr (hex 8 chars)"));
  output->println(F("(an) set 1.0/1.1  NwkSKey / NwkSEncKey"));
  output->println(F("(aa) set 1.0/1.1 AppSKey"));
  output->println(F("(af) set 1.1 FNwkSIntKey"));
  output->println(F("(as) set 1.1 SNwkSIntKey"));
  output->println(F("(ae) session information"));
  output->println(F("(ar) reset session and start ABP"));
  output->println(F("settings, read with (ai), written as e.g. T9"));
  printParameterHelp(output, PARAM_LORAWAN_SLEEP_SECONDS,
                     F("seconds of sleep after an uplink, 0 = stay awake"));
  printParameterHelp(output, PARAM_LORAWAN_INTERVAL_SECONDS,
                     F("extra seconds to wait after an uplink"));
  printParameterHelp(output, PARAM_LORAWAN_SPREADING_FACTOR,
                     F("spreading factor 7-12, other value = ADR"));
  output->println(F("    the two delays under 10s are raised to 10s, and the"));
  output->println(F("    duty cycle still delays the uplink further if needed"));
}

void updateLoRaParameters() {
  // Load devAddr as bytes to maintain proper byte order
  devAddr = getNVSParameterInt32("lora.devAddr");
  getBlobParameter("lora.appSKey", appSKey, sizeof(appSKey));
  getBlobParameter("lora.nwkSEncKey", nwkSEncKey, sizeof(nwkSEncKey));
  getBlobParameter("lora.fNwkSIntK", fNwkSIntKey, sizeof(fNwkSIntKey));
  getBlobParameter("lora.sNwkSIntK", sNwkSIntKey, sizeof(sNwkSIntKey));
}

// we automatically detect version 1.0 or 1.1 based on the keys if length
// = 16 bytes
int8_t getLoraVersion() {
  if (getBlobParameter("lora.fNwkSIntK", fNwkSIntKey, sizeof(fNwkSIntKey)) &&
      getBlobParameter("lora.sNwkSIntK", sNwkSIntKey, sizeof(sNwkSIntKey))) {
    return 1;
  }
  return 0;
}

void processLoraCommand(char command,
                        char* paramValue,
                        Print* output) {  // char and char* ??
  if (command != 'i' && command != 'n' && command != 'a' && command != 'e' &&
      command != 'f' && command != 's' && command != 'd' && command != 'r') {
    printLoraWanHelp(output);
    return;
  }

  /* the whole command runs under the mutex: it touches the node, the shared key
     arrays and the shared blob buffer in params.cpp */
  if (!takeLoraNode(output)) {
    return;
  }

  switch (command) {
    case 'i':
      updateLoRaParameters();
      output->println(F("LoRaWAN information"));
      if (getLoraVersion() == 1) {
        output->println(F("LoRaWAN 1.1"));
      } else {
        output->println(F("LoRaWAN 1.0"));
      }
      // and we print information and keys
      output->print(F("DevAddr: "));
      output->println(devAddr, HEX);
      output->print(F("AppSKey: "));
      toHex(output, appSKey, sizeof(appSKey));
      output->println();
      output->print(F("NwkSEncKey: "));
      toHex(output, nwkSEncKey, sizeof(nwkSEncKey));
      output->println();
      output->print(F("FNwkSIntKey: "));
      toHex(output, fNwkSIntKey, sizeof(fNwkSIntKey));
      output->println();
      output->print(F("SNwkSIntKey: "));
      toHex(output, sNwkSIntKey, sizeof(sNwkSIntKey));
      output->println();
      output->print(F("Uplink interval: "));
      output->println(getParameter(PARAM_LORAWAN_INTERVAL_SECONDS));
      output->print(F("Sleep seconds: "));
      output->println(getParameter(PARAM_LORAWAN_SLEEP_SECONDS));
      printSpreadingFactor(output);
      output->print(F("Next uplink allowed in: "));
      output->print(node.timeUntilUplink() / 1000);
      output->println(F("s"));
      output->print(F("FCntUp (last uplink): "));
      output->println(node.getFCntUp());
      break;
    case 'n':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.nwkSEncKey");
        break;
      }
      setBlobParameterFromHex("lora.nwkSEncKey", paramValue);
      output->println(paramValue);
      break;
    case 'a':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.appSKey");
        break;
      }
      setBlobParameterFromHex("lora.appSKey", paramValue);
      output->println(paramValue);
      break;
    case 'e':
      printLoraSession(output);
      break;
    case 'f':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.fNwkSIntK");
        break;
      }
      setBlobParameterFromHex("lora.fNwkSIntK", paramValue);
      output->println(paramValue);
      break;
    case 's':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.sNwkSIntK");
        break;
      }
      setBlobParameterFromHex("lora.sNwkSIntK", paramValue);
      output->println(paramValue);
      break;
    case 'd': {
      if (!checkParameterLength(paramValue, 8, output)) {
        deleteParameter("lora.devAddr");
        break;
      }
      /* strtol saturates at 0x7FFFFFFF, which silently corrupts any DevAddr
         with the top bit set */
      setNVSParameterInt32("lora.devAddr",
                           (int32_t)strtoul(paramValue, NULL, 16));
      output->println(paramValue);
      break;
    }
    case 'r':
      resetLoraSession(output);
      if (loraState == RADIOLIB_ERR_NONE) {
        output->println(F("LoRaWAN session reset and activated"));
      } else {
        output->println(F("LoRaWAN session reset failed"));
      }
      break;
  }

  xSemaphoreGive(xSemaphoreLora);
}

void taskLoraWanSend() {
  xSemaphoreLora = xSemaphoreCreateMutexStatic(&xMutexBufferLora);
  xTaskCreatePinnedToCore(TaskLoraWanSend, "TaskLoraWanSend",
                          16000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif