#include "config.h"
#ifdef THR_DRONE_ID
#include "droneIdDecode.h"
#include "droneIdFrames.h"

/* The bounds check and the call into the reference decoder both live in
   droneIdFrames.cpp, where the rest of the framing is and where a host test can
   reach them; this only unwraps the capture. */
ODID_messagetype_t droneIdDecode(const DroneCapture* capture,
                                 ODID_UAS_Data* record) {
  return droneIdDecodePayload(capture->payload, capture->length, record);
}

boolean droneIdDecodeHasData(const ODID_UAS_Data* record) {
  if (record->LocationValid || record->SelfIDValid || record->SystemValid ||
      record->OperatorIDValid) {
    return true;
  }
  for (uint8_t i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
    if (record->BasicIDValid[i]) {
      return true;
    }
  }
  for (uint8_t i = 0; i < ODID_AUTH_MAX_PAGES; i++) {
    if (record->AuthValid[i]) {
      return true;
    }
  }
  return false;
}

const ODID_BasicID_data* droneIdDecodeBasicId(const ODID_UAS_Data* record) {
  for (uint8_t i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
    if (record->BasicIDValid[i] && record->BasicID[i].UASID[0] != '\0') {
      return &record->BasicID[i];
    }
  }
  return NULL;
}

boolean droneIdDecodeIdIsText(ODID_idtype_t idType) {
  return idType == ODID_IDTYPE_SERIAL_NUMBER ||
         idType == ODID_IDTYPE_CAA_REGISTRATION_ID;
}

boolean droneIdDecodeHasPosition(double latitude, double longitude) {
  return latitude != 0.0 || longitude != 0.0;
}
#endif
