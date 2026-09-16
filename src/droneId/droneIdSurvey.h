#ifndef _DRONE_ID_SURVEY_H
#define _DRONE_ID_SURVEY_H

#include <Arduino.h>

/* What was on the air that was NOT Remote ID.

   Without this, a drone the decoder does not understand and a sky with no
   drone in it look exactly alike: the locators reject an unrecognised frame
   before it becomes a payload, so no counter moves and (dh) has nothing to
   show. That is the wrong answer to give somebody standing under a drone they
   can see.

   So (dv) turns on a survey of the identifiers that went past: every vendor
   element in a Wi-Fi beacon by its OUI, and every Bluetooth service data UUID,
   counted. It is a commissioning aid, off by default and never persisted -
   walking every beacon's elements is work that a deployed board should not be
   doing, and the table would fill with the building's own access points.

   The two identifiers worth recognising on sight are named when printed:
   FA:0B:BC is ASD-STAN's, which is Remote ID and should have been decoded, and
   26:37:12 is DJI's own, which is the proprietary DroneID this firmware does
   not decode - see docs/drone-rf-detection.md. Seeing the second and not the
   first is the answer to "why is nothing detected". */

void droneIdSurveyEnable(boolean enabled);
boolean droneIdSurveyEnabled();

/* Called from the radio callbacks, and only while the survey is on. */
void droneIdSurveyBeacon(const uint8_t* frame, uint16_t length);
void droneIdSurveyAdvertisement(const uint8_t* payload, size_t length);

void droneIdSurveyPrint(Print* output);
void droneIdSurveyReset();

#endif
