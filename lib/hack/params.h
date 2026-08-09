#pragma once

#include "config.h"

/* The most slots one console command may name. A LoRa frame holds 20 values,
   and a command typed on a port has no reason to be longer. */
#define MAX_PARAM_ASSIGNMENTS 20

/* One slot a console command named, and the value it gave it.

   "A1,2,3" is three of these, "A1,C3" is two on unrelated slots, and a bare "A"
   is one with no value at all - a read. This is the whole grammar of the
   parameter console, and it is shared: the serial port applies the assignments
   where it stands, and the mesh (ax) groups them into runs and puts them in a
   frame. Two parsers for one syntax is how the two drift apart. */
typedef struct {
  /* Zero-based parameter index. */
  uint8_t slot;
  /* False for a read: "A" prints the slot, "A1" stores 1 in it. */
  boolean hasValue;
  int16_t value;
} ParameterAssignment;

/* Reads a parameter command into assignments.

   Handles the I2C form too: "55D123" is slot D on wire device 55, which is why
   the address comes back rather than being refused - the digits before a letter
   are a device, the digits after it a value.
   Returns the number of assignments, or 0 when the text is not one. */
uint8_t parseParameterAssignments(const char* text,
                                  ParameterAssignment* out,
                                  uint8_t maxCount,
                                  uint8_t* wireAddress);

boolean getParameterBit(byte number, byte bitToRead);
boolean setParameterBit(byte number, byte bitToSet);
boolean clearParameterBit(byte number, byte bitToClear);

boolean checkParameterLength(char* paramValue, int length, Print* output);

void deleteParameter(const char* key);
void setBlobParameterFromHex(const char* key, const char* hexString);
boolean getBlobParameter(const char* key, uint8_t* blob, size_t length);
void setBlobParameter(const char* key, uint8_t* blob, size_t length);

// will be saved in NVS
void setNVSParameterInt32(const char* key, int32_t value);
int32_t getNVSParameterInt32(const char* key);

void setParameterInt32(byte numberLow, byte numberHigh, int32_t value);
int32_t getParameterInt32(byte numberLow, byte numberHigh);

/* An int32 kept in two adjacent parameters, low half first. Adjacency is what
   lets a block of parameters carry it: (ac) and the periodic broadcast send a
   run of consecutive slots, so the two halves travel in the same frame. */
void setParameterInt32(byte number, int32_t value);
int32_t getParameterInt32(byte number);

void setupParameters();

void toggleParameterBit(byte number, byte bitToToggle);

int16_t getParameter(byte number);

void setParameter(byte number, int16_t value);

void incrementParameter(byte number);

void saveParameters();

void setAndSaveParameter(byte number, int16_t value);

boolean saveAndLogError(boolean isError, byte errorFlag);

void printParameter(Print* output, byte number);

String numberToLabel(byte number);

/* prints one settings line of a help screen, e.g. "(T) spreading factor". The
   letter is derived from the parameter number because the same parameter does
   not have the same number, hence not the same letter, on every board */
void printParameterHelp(Print* output,
                        byte number,
                        const __FlashStringHelper* description);

void printParameters(Print* output);

void printCompactParameters(Print* output, byte number);

void printCompactParameters(Print* output);

void setParameter(const char* key, char* value);
void getParameter(const char* key, char* value);

void setQualifier(int16_t value);

int getQualifier();

#define ERROR_VALUE -32768