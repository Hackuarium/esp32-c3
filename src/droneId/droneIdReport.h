#ifndef _DRONE_ID_REPORT_H
#define _DRONE_ID_REPORT_H

#include <Arduino.h>

#include "droneIdTable.h"

/* Everything this board prints about an aircraft. Every function here takes a
   row that has already been copied out of the table, so none of them holds the
   lock while it writes to a serial port. */

/* The whole of what is known: printed on a first sighting, and again whenever
   a message type arrives that this transmitter had not sent before - which is
   how the identity, the operator's position and the operator's registration
   turn up, seconds or minutes apart. */
void droneIdReportAircraft(Print* output, const DroneAircraft* entry,
                           boolean isNew);

/* One line, at most every (D) seconds: where it is now. */
void droneIdReportPosition(Print* output, const DroneAircraft* entry);

/* It has stopped transmitting, or flown out of range - which are the same
   thing from here. */
void droneIdReportLost(Print* output, const DroneAircraft* entry);

void droneIdReportList(Print* output);
void droneIdReportDetail(Print* output, uint8_t index);

void droneIdPrintAddress(Print* output, const uint8_t* address);
const __FlashStringHelper* droneIdSourceLabel(uint8_t source);

#endif
