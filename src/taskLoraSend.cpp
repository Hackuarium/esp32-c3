#include "config.h"
#include "params.h"
#include "taskLoraConfig.h"

#define BAND 868E6

void deepSleep(int seconds);
void waitOrSleep();

void TaskLoraSend(void* pvParameters) {
  vTaskDelay(5 * 1000);
  int16_t state = radio.begin();
  debug(state != RADIOLIB_ERR_NONE, F("Initialise radio failed"), state, true);
  state = node.beginABP(devAddr, NULL, NULL, nwkSEncKey, appSKey);
  debug(state != RADIOLIB_ERR_NONE, F("Initialise node failed"), state, true);

  node.activateABP();

  debug(state != RADIOLIB_ERR_NONE, F("Activate ABP failed"), state, true);

  // code from:
  // https://github.com/jgromes/RadioLib/blob/master/examples/LoRaWAN/LoRaWAN_ABP/LoRaWAN_ABP.ino

  while (true) {
    uint8_t uplinkPayload[12];
    for (int i = 0; i < 6; i++) {
      int16_t value = getParameter(i);
      uplinkPayload[i * 2] = lowByte(value);
      uplinkPayload[i * 2 + 1] = highByte(value);
    }

    int state = node.sendReceive(uplinkPayload, sizeof(uplinkPayload));
    debug(state < RADIOLIB_ERR_NONE, F("Error in sendReceive"), state, false);

    // Check if a downlink was received
    // (state 0 = no downlink, state 1/2 = downlink in window Rx1/Rx2)
    if (state > 0) {
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
      60) {
    vTaskDelay(60 * 1000);
  } else if (getParameter(PARAM_LORA_INTERVAL_SECONDS) > 0) {
    vTaskDelay(getParameter(PARAM_LORA_INTERVAL_SECONDS) * 1000);
  }
}

void printLoRaHelp(Print* output) {
  output->println(F("(av) LoRaWAN version [0=1.0 1=1.1]"));
  output->println(F("(ai) info - print keys, version"));
  output->println(F("(an) set 1.0/1.1  NwkSKey / NwkSEncKey"));
  output->println(F("(aa) set 1.0/1.1 AppSKey"));
  output->println(F("(af) set 1.1 FNwkSIntKey"));
  output->println(F("(as) set 1.1 SNwkSIntKey"));
  output->println(F("(ad) set DevAddr (hex 8 chars)"));
}

void updateLoRaParameters(Print* output) {
  getBlobParameter("lora.devAddr", (uint8_t*)&devAddr, sizeof(devAddr));
  getBlobParameter("lora.appSKey", appSKey, sizeof(appSKey));
  getBlobParameter("lora.nwkSEncKey", nwkSEncKey, sizeof(nwkSEncKey));
}

void processLoraCommand(char command,
                        char* paramValue,
                        Print* output) {  // char and char* ??
  switch (command) {
    case 'v':
      // check parameter length is 1 and we convert to a value and setParameter

      output->print(F("LoRaWAN version: "));
      output->println(getParameter(PARAM_LORA_WAN_VERSION));
      break;
    case 'i':
      // we automatically detect version 1.0 or 1.1 based on the keys
      // and we print information and keys

      output->print(F("DevAddr: "));
      output->println(String(devAddr, HEX));
      output->print(F("AppSKey: "));
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
    case 'f':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.fNwkSIntKey");
        return;
      }
      setBlobParameterFromHex("lora.fNwkSIntKey", paramValue);
      output->println(paramValue);
      break;
    case 's':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("lora.sNwkSIntKey");
        return;
      }
      setBlobParameterFromHex("lora.sNwkSIntKey", paramValue);
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
