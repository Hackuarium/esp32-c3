#include "config.h"
#if BOARD_TYPE == KIND_LORA_MESH
#include "params.h"

void taskBlink();
void taskSerial();
void taskGPS();
void taskBLE();
void taskLoraMesh();
void loraMeshResetParameters();
void taskAnalogInput();
void taskBMP280();
void taskAHTx0();

void setupLoraMesh() {
  setupParameters();
  taskSerial();

#ifdef GPS_RX
  taskGPS();
#endif
#ifdef THR_BLE
  taskBLE();
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

  loraMeshResetParameters();

#ifdef PARAM_TELEMETRY_FIRST
  /* a tracker is only a node whose broadcast window happens to be the eight GPS
     slots: latitude, longitude, altitude, satellites, HDOP and fix quality -
     nine when the board also watches a Bluetooth beacon, whose RSSI sits in the
     slot right after them so that one run of parameters carries both.

     60 s is where a reset leaves the cadence, and the only place it is decided:
     it is a parameter afterwards, changed with (gt) or DF on a running node,
     never a build flag - a firmware that hard-coded it would silently disagree
     with the node it was flashed onto.

     It follows from the sub-band rather than from what a tracker would like:
     868.4 allows 1% of an hour, which is 36 s of airtime, and one 29-byte
     telemetry frame costs 226 ms at SF9/125 kHz. 60 s spends 13.6 s of the 36,
     20 s would ask for 40.7 - past the whole allowance, and the governor drops
     what it cannot pay for rather than transmitting it late. The beacon's RSSI
     makes the frame 31 bytes and 247 ms, so 14.8 s of the same 36. */
  setAndSaveParameter(PARAM_LORA_INTERVAL_SECONDS, 60);
  setAndSaveParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER,
                      PARAM_TELEMETRY_FIRST);
  setAndSaveParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS,
                      PARAM_TELEMETRY_BLOCK_SIZE);
#endif

#ifdef THR_BLE
  /* the exponent has a defensible default - free space - but the reference at
     1 m does not, and a wrong one is a distance that reads like a measurement
     while pointing at the wrong field. It stays unset until (bk) measures it. */
  setAndSaveParameter(PARAM_BLE_PATH_LOSS, BLE_PATH_LOSS_DEFAULT);
  setAndSaveParameter(PARAM_BLE_REPORT_SECONDS, BLE_REPORT_SECONDS_DEFAULT);
#endif

  setQualifier(7275);
}

#endif
