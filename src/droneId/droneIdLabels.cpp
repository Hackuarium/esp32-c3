#include "config.h"
#ifdef THR_DRONE_ID
#include "droneIdLabels.h"

/* Tables rather than switches: every one of these is a dense enumeration
   starting at zero, so the value is the index and anything past the end is
   reserved for a revision of the standard that has not been written yet. */

static const char* lookup(const char* const* labels, uint8_t count,
                          uint8_t value, const char* beyond) {
  return value < count ? labels[value] : beyond;
}

static const char* const idTypes[] = {
    "none", "serial number", "CAA registration", "UTM UUID", "session id"};

static const char* const uaTypes[] = {
    "undeclared",   "aeroplane",        "multirotor",      "gyroplane",
    "hybrid VTOL",  "ornithopter",      "glider",          "kite",
    "free balloon", "captive balloon",  "airship",         "parachute",
    "rocket",       "tethered powered", "ground obstacle", "other"};

static const char* const statuses[] = {"undeclared", "on ground", "airborne",
                                       "EMERGENCY", "remote ID failure"};

static const char* const operatorLocations[] = {"takeoff point", "live GNSS",
                                                "fixed"};

static const char* const categories[] = {"undeclared", "open", "specific",
                                         "certified"};

/* The wire value is the class number plus one - C0 travels as 1 - so printing
   the nibble would name every aircraft one class above what it is. */
static const char* const classes[] = {"undeclared", "C0", "C1", "C2",
                                      "C3",         "C4", "C5", "C6"};

static const char* const authTypes[] = {"none",
                                        "UAS ID signature",
                                        "operator ID signature",
                                        "message set signature",
                                        "network remote ID",
                                        "specific method"};

const char* droneIdIdTypeLabel(uint8_t idType) {
  return lookup(idTypes, 5, idType, "reserved");
}

const char* droneIdUaTypeLabel(uint8_t uaType) {
  return lookup(uaTypes, 16, uaType, "undeclared");
}

const char* droneIdStatusLabel(uint8_t status) {
  return lookup(statuses, 5, status, "reserved");
}

const char* droneIdOperatorLocationLabel(uint8_t type) {
  return lookup(operatorLocations, 3, type, "reserved");
}

const char* droneIdCategoryLabel(uint8_t category) {
  return lookup(categories, 4, category, "reserved");
}

const char* droneIdClassLabel(uint8_t classEu) {
  return lookup(classes, 8, classEu, "reserved");
}

const char* droneIdAuthTypeLabel(uint8_t authType) {
  return lookup(authTypes, 6, authType,
                authType < 0x0A ? "reserved" : "private");
}

/* A bound in metres, written the way somebody reads a distance: kilometres
   past a kilometre, metres below it. */
static void printBound(Print* output, float metres) {
  output->print('<');
  if (metres >= 1000.0f) {
    output->print(metres / 1000.0f, 3);
    output->print(F(" km"));
  } else {
    output->print(metres, metres < 10.0f ? 1 : 0);
    output->print(F(" m"));
  }
}

void droneIdPrintHorizontalAccuracy(Print* output, uint8_t accuracy) {
  if (accuracy == ODID_HOR_ACC_UNKNOWN || accuracy > ODID_HOR_ACC_1_METER) {
    output->print(F("unknown"));
    return;
  }
  /* Upstream returns 7808 m for the four nautical mile bound, where four times
     1852 is 7408 - which is what its own header comment says. Corrected here
     rather than reported, because it is printed as a distance somebody may act
     on; see decodeHorizontalAccuracy() in lib/opendroneid/opendroneid.c. */
  printBound(output, accuracy == ODID_HOR_ACC_4NM
                         ? 7408.0f
                         : decodeHorizontalAccuracy(
                               (ODID_Horizontal_accuracy_t)accuracy));
}

void droneIdPrintVerticalAccuracy(Print* output, uint8_t accuracy) {
  if (accuracy == ODID_VER_ACC_UNKNOWN || accuracy > ODID_VER_ACC_1_METER) {
    output->print(F("unknown"));
    return;
  }
  printBound(output,
             decodeVerticalAccuracy((ODID_Vertical_accuracy_t)accuracy));
}

void droneIdPrintSpeedAccuracy(Print* output, uint8_t accuracy) {
  if (accuracy == ODID_SPEED_ACC_UNKNOWN ||
      accuracy > ODID_SPEED_ACC_0_3_METERS_PER_SECOND) {
    output->print(F("unknown"));
    return;
  }
  output->print('<');
  output->print(decodeSpeedAccuracy((ODID_Speed_accuracy_t)accuracy), 1);
  output->print(F(" m/s"));
}
#endif
