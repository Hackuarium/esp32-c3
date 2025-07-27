#include "config.h"
#include "params.h"
#include "taskLoraConfig.h"
#include "toHex.h"

#define BAND 868E6

void deepSleep(int seconds);

void waitOrSleep();
int8_t getLoraVersion();
void resetLoraSession();
uint8_t* loraSession;
int16_t loraState = 0;

void initLoraSession() {
  // we try to reload the session from NVS if it exists and
  // has the correct size
  size_t sessionSize = RADIOLIB_LORAWAN_SESSION_BUF_SIZE;
  boolean valid = getBlobParameter("lora.session", loraSession,
                                   RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  if (valid) {
    node.setBufferSession(loraSession);
  } else {
    resetLoraSession();
  }
}

void resetLoraSession() {
  if (getLoraVersion() == 0) {
  }
}

void printLoraSession(Print* output) {
  output->print("Session: ");
  // print all the sssion parameters from RadioLib extracteing the info from
  // the LoRaWANSession_t enum
  output->print(F("DevAddr: "));
  // need to display it as uint32_t
  output->println(*(uint32_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_DEV_ADDR),
                  HEX);
  output->print(F("AppSKey: "));
  toHex(output, loraSession + RADIOLIB_LORAWAN_SESSION_APP_SKEY,
        RADIOLIB_AES128_KEY_SIZE);
  output->println();
  output->print(F("NwkSEncKey: "));
  toHex(output, loraSession + RADIOLIB_LORAWAN_SESSION_NWK_SENC_KEY,
        RADIOLIB_AES128_KEY_SIZE);
  output->println();
  output->print(F("FNwkSIntKey: "));
  toHex(output, loraSession + RADIOLIB_LORAWAN_SESSION_FNWK_SINT_KEY,
        RADIOLIB_AES128_KEY_SIZE);
  output->println();
  output->print(F("SNwkSIntKey: "));
  toHex(output, loraSession + RADIOLIB_LORAWAN_SESSION_SNWK_SINT_KEY,
        RADIOLIB_AES128_KEY_SIZE);
  output->println();
  output->print(F("FCntUp: "));
  output->print(*(uint16_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_FCNT_UP));
  output->println();
  output->print(F("N_FCntDown: "));
  output->print(
      *(uint32_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_N_FCNT_DOWN));
  output->println();
  output->print(F("A_FCntDown: "));
  output->print(
      *(uint32_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_A_FCNT_DOWN));
  output->println();
  output->print(F("ADR_FCnt: "));
  output->print(*(uint32_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_ADR_FCNT));
  output->println();
  output->print(F("ConfFCntUp: "));  // 4 bytes
  output->print(
      *(uint32_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_CONF_FCNT_UP));
  output->println();
  output->print(F("ConfFCntDown: "));  // 4 bytes
  output->print(
      *(uint32_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_CONF_FCNT_DOWN));
  output->println();
  output->print(F("RJCount0: "));
  output->print(*(uint16_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_RJ_COUNT0));
  output->println();
  output->print(F("RJCount1: "));
  output->print(*(uint16_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_RJ_COUNT1));
  output->println();
  output->print(F("HomenetID: "));
  output->println(
      *(uint32_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_HOMENET_ID), HEX);
  output->print(F("Version: "));
  output->print(*(uint8_t*)(loraSession + RADIOLIB_LORAWAN_SESSION_VERSION));
  output->println();
}

void TaskLoraSend(void* pvParameters) {
  vTaskDelay(5 * 1000);
  loraState = radio.begin();
  debug(loraState != RADIOLIB_ERR_NONE, F("Initialise radio failed"), loraState,
        true);
  loraSession = node.getBufferSession();
  loraState = node.beginABP(devAddr, NULL, NULL, nwkSEncKey, appSKey);
  debug(loraState != RADIOLIB_ERR_NONE, F("Initialise node failed"), loraState,
        true);

  node.activateABP();

  debug(loraState != RADIOLIB_ERR_NONE, F("Activate ABP failed"), loraState,
        true);

  // code from:
  // https://github.com/jgromes/RadioLib/blob/master/examples/LoRaWAN/LoRaWAN_ABP/LoRaWAN_ABP.ino

  while (true) {
    uint8_t uplinkPayload[12];
    for (int i = 0; i < 6; i++) {
      int16_t value = getParameter(i);
      uplinkPayload[i * 2] = lowByte(value);
      uplinkPayload[i * 2 + 1] = highByte(value);
    }

    int loraState = node.sendReceive(uplinkPayload, sizeof(uplinkPayload));
    debug(loraState < RADIOLIB_ERR_NONE, F("Error in sendReceive"), loraState,
          false);

    setBlobParameter("lora.session", loraSession,
                     RADIOLIB_LORAWAN_SESSION_BUF_SIZE);

    // Check if a downlink was received
    // (loraState 0 = no downlink, loraState 1/2 = downlink in window Rx1/Rx2)
    if (loraState > 0) {
      //  Serial.println(F("Received a downlink"));
    } else {
      //  Serial.println(F("No downlink received"));
    }
    waitOrSleep();
  }
}

void waitOrSleep() {
  // should we sleep ?
  if (getParameter(PARAM_LORA_SLEEP_SECONDS) > 0) {
    // we should sleep if uptime is more than 300s. Too difficult to debug
    // otherwise
    if (millis() > 300 * 1000) {
      deepSleep(getParameter(PARAM_LORA_SLEEP_SECONDS));
    } else {
      vTaskDelay(getParameter(PARAM_LORA_SLEEP_SECONDS) * 1000);
    }
  }

  // we wait for the next uplink interval
  // this is to avoid sending too often and to respect the FUP
  if (getParameter(PARAM_LORA_SLEEP_SECONDS) +
          getParameter(PARAM_LORA_INTERVAL_SECONDS) <
      10) {
    vTaskDelay(10 * 1000);
  } else if (getParameter(PARAM_LORA_INTERVAL_SECONDS) > 0) {
    vTaskDelay(getParameter(PARAM_LORA_INTERVAL_SECONDS) * 1000);
  }
}

void printLoRaHelp(Print* output) {
  output->println(F("(ai) info - print keys, version"));
  output->println(F("(ad) set DevAddr (hex 8 chars)"));
  output->println(F("(an) set 1.0/1.1  NwkSKey / NwkSEncKey"));
  output->println(F("(aa) set 1.0/1.1 AppSKey"));
  output->println(F("(af) set 1.1 FNwkSIntKey"));
  output->println(F("(as) set 1.1 SNwkSIntKey"));
  output->println(F("(ae) session information"));
}

void updateLoRaParameters() {
  getBlobParameter("lora.devAddr", (uint8_t*)&devAddr, sizeof(devAddr));
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
      toHex(output, (uint8_t*)&devAddr, sizeof(devAddr));
      output->println();
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
      output->println(getParameter(PARAM_LORA_INTERVAL_SECONDS));
      output->print(F("Sleep seconds: "));
      output->println(getParameter(PARAM_LORA_SLEEP_SECONDS));
      break;
    case 'n':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.nwkSEncKey");
        return;
      }
      setBlobParameterFromHex("lora.nwkSEncKey", paramValue);
      output->println(paramValue);
      break;
    case 'a':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.appSKey");
        return;
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
        return;
      }
      setBlobParameterFromHex("lora.fNwkSIntK", paramValue);
      output->println(paramValue);
      break;
    case 's':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.sNwkSIntK");
        return;
      }
      setBlobParameterFromHex("lora.sNwkSIntK", paramValue);
      output->println(paramValue);
      break;
    case 'd': {
      if (!checkParameterLength(paramValue, 8, output)) {
        deleteParameter("lora.devAddr");
        return;
      }
      setBlobParameterFromHex("lora.devAddr", paramValue);
      output->println(paramValue);
      break;
    }
    default:
      printLoRaHelp(output);
      break;
  }
}

void taskLoraSend() {
  xTaskCreatePinnedToCore(TaskLoraSend, "TaskLoraSend",
                          16000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
