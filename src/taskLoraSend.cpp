#include "config.h"
#include "params.h"
#include "taskLoraConfig.h"

#define BAND 868E6

void deepSleep(int seconds);

void TaskLoraSend(void* pvParameters) {
  vTaskDelay(5 * 1000);
  Serial.println(F("Initialise the radio"));
  int16_t state = radio.begin();
  debug(state != RADIOLIB_ERR_NONE, F("Initialise radio failed"), state, true);
  state = node.beginABP(devAddr, NULL, NULL, nwkSKey, appSKey);
  debug(state != RADIOLIB_ERR_NONE, F("Initialise node failed"), state, true);

  node.activateABP();

  debug(state != RADIOLIB_ERR_NONE, F("Activate ABP failed"), state, true);

  Serial.println(F("Ready!\n"));

  // code from:
  // https://github.com/jgromes/RadioLib/blob/master/examples/LoRaWAN/LoRaWAN_ABP/LoRaWAN_ABP.ino

  while (true) {
    Serial.println(F("Sending uplink"));

    uint8_t value1 = radio.random(100);
    uint16_t value2 = radio.random(2000);

    // Build payload byte array
    uint8_t uplinkPayload[12];
    for (int i = 0; i < 6; i++) {
      int16_t value = getParameter(i);
      uplinkPayload[i * 2] = highByte(value);
      uplinkPayload[i * 2 + 1] = lowByte(value);
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

    // Serial.print(F("Next uplink in "));
    // Serial.print(uplinkIntervalSeconds);
    // Serial.println(F(" seconds\n"));

    vTaskDelay(uplinkIntervalSeconds * 1000);
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
