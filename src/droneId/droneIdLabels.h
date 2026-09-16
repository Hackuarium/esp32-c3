#ifndef _DRONE_ID_LABELS_H
#define _DRONE_ID_LABELS_H

#include <Arduino.h>
#include <opendroneid.h>

/* What the enumerations mean, in words short enough to sit in a column next to
   a position and a signal strength.

   lib/opendroneid has printXxx_data() functions of its own and they are not
   used: they print raw enumeration numbers with printf to stdout - "UAType: 2",
   "Status: 2" - which is a developer's dump of a struct rather than something
   an operator reads, and they cannot be pointed at a Print stream. Naming is
   presentation; every byte of the decoding is still upstream's. */

const char* droneIdIdTypeLabel(uint8_t idType);
const char* droneIdUaTypeLabel(uint8_t uaType);
const char* droneIdStatusLabel(uint8_t status);
const char* droneIdOperatorLocationLabel(uint8_t type);
const char* droneIdCategoryLabel(uint8_t category);
const char* droneIdClassLabel(uint8_t classEu);
const char* droneIdAuthTypeLabel(uint8_t authType);

/* The accuracies are upper bounds and are printed as such - a horizontal
   accuracy of 9 means the aircraft believes it is within 30 m, not that it is
   30 m out. Zero means both "unknown" and "worse than the coarsest bound",
   which the standard does not separate, so neither does this. */
void droneIdPrintHorizontalAccuracy(Print* output, uint8_t accuracy);
void droneIdPrintVerticalAccuracy(Print* output, uint8_t accuracy);
void droneIdPrintSpeedAccuracy(Print* output, uint8_t accuracy);

#endif
