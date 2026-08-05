#include "config.h"
#if BOARD_TYPE == KIND_LORA_MESH
#include "params.h"

void taskBlink();
void taskSerial();
void taskGPS();
void taskLoraMesh();
void taskAnalogInput();
void taskBMP280();
void taskAHTx0();

void setupLoraMesh() {
  setupParameters();
  taskSerial();

#ifdef GPS_RX
  taskGPS();
#endif
  taskLoraMesh();

  // taskAnalogInput();
  // taskBMP280();
  // taskAHTx0();

  taskBlink();
}

void loopLoraMesh() {
  vTaskDelay(100000);
}

void resetParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    setAndSaveParameter(i, ERROR_VALUE);
  }

  setAndSaveParameter(PARAM_LORA_ROLE, LORA_ROLE_ENDPOINT);
  setAndSaveParameter(PARAM_LORA_TTL, 2);
  setAndSaveParameter(PARAM_LORA_SPREADING_FACTOR, 7);
  setAndSaveParameter(PARAM_LORA_FREQUENCY, 8684);
  setAndSaveParameter(PARAM_LORA_BANDWIDTH, 125);
  setAndSaveParameter(PARAM_LORA_HELLO_SECONDS, LORA_HELLO_SECONDS_DEFAULT);

#ifdef GPS_RX
  /* a tracker is only a node whose broadcast window happens to be the eight GPS
     slots: latitude, longitude, altitude, satellites, HDOP and fix quality */
  setAndSaveParameter(PARAM_LORA_INTERVAL_SECONDS, 60);
  setAndSaveParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER, PARAM_GPS_LATITUDE);
  setAndSaveParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS, 8);
#else
  setAndSaveParameter(PARAM_LORA_INTERVAL_SECONDS, 0);
  setAndSaveParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER, 0);
  setAndSaveParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS, 0);
#endif

  /* the address and the group key stay untouched: they are the node identity,
     and a reset that silently wiped them would take the node off the mesh */

  setQualifier(7275);
}

#endif
