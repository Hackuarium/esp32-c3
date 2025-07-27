#include "params.h"
#include <ArduinoNvs.h>
#include "./SerialUtilities.h"
#include "./taskNTPD.h"
#include "./toHex.h"
#include "config.h"

#define INT_MAX_VALUE 32767
#define INT_MIN_VALUE -32768
#define LONG_MAX_VALUE 2147483647

// value that should not be taken into account
// in case of error the parameter is set to this value
#define ERROR_VALUE -32768

RTC_DATA_ATTR int16_t parameters[MAX_PARAM];

uint8_t tempBlob[64];  // temporary buffer for blobs

// todo uint16_t getQualifier();
boolean setParameterBit(byte number, byte bitToSet);
boolean clearParameterBit(byte number, byte bitToClear);
String numberToLabel(byte number);
void checkParameters();

boolean getParameterBit(byte number, byte bitToRead) {
  return (parameters[number] >> bitToRead) & 1;
}

boolean setParameterBit(byte number, byte bitToSet) {
  if (getParameterBit(number, bitToSet))
    return false;
  parameters[number] |= 1 << bitToSet;
  return true;
}

boolean clearParameterBit(byte number, byte bitToClear) {
  if (!getParameterBit(number, bitToClear))
    return false;
  parameters[number] &= ~(1 << bitToClear);
  return true;
}

void toggleParameterBit(byte number, byte bitToToggle) {
  parameters[number] ^= 1 << bitToToggle;
}

void setupParameters() {
  NVS.begin();
  for (byte i = 0; i < MAX_PARAM; i++) {
    parameters[i] = NVS.getInt(numberToLabel(i));
  }
  checkParameters();
}

void setParameter(const char* key, char* value) {
  NVS.setString(key, value);
}

void getParameter(const char* key, char* value) {
  strcpy(value, NVS.getString(key).c_str());
}

/**
 * Parse a hex string into tempBlob
 * String can starts with 0x and containg spaces
 */
void parseHex(char* hex, uint8_t* tempBlob) {
  memset(tempBlob, 0, sizeof(tempBlob));
  size_t len = strlen(hex);
  for (size_t i = 0; i < len; i += 2) {
    String byteString = String(hex[i]) + String(hex[i + 1]);
    tempBlob[i / 2] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
  }
}

void deleteParameter(const char* key) {
  NVS.erase(key);
}

void setBlobParameterFromHex(const char* key, char* hexString) {
  parseHex(hexString, tempBlob);
  NVS.setBlob(key, tempBlob, strlen(hexString) / 2);
}

boolean getBlobParameter(const char* key, uint8_t* blob, size_t length) {
  if (length > sizeof(tempBlob)) {
    length = sizeof(tempBlob);
  }
  boolean isPresent = NVS.getBlob(key, tempBlob, length);
  memcpy(blob, tempBlob, length);
  return isPresent;
}

int16_t getParameter(byte number) {
  return parameters[number];
}

int32_t getParameterInt32(byte numberLow, byte numberHigh) {
  return (int32_t)parameters[numberLow] & 0xFFFF |
         ((int32_t)(parameters[numberHigh] & 0xFFFF) << 16);
}

void setParameterInt32(byte numberLow, byte numberHigh, int32_t value) {
  parameters[numberLow] = value & 0xFFFF;
  parameters[numberHigh] = (value >> 16) & 0xFFFF;
}

void setParameter(byte number, int16_t value) {
  parameters[number] = value;
}

void incrementParameter(byte number) {
  parameters[number]++;
}

void saveParameters() {
  for (byte i = 0; i < MAX_PARAM; i++) {
    NVS.setInt(numberToLabel(i), parameters[i]);
  }
}

/*
  This will take time, around 4 ms
  This will also use the EEPROM that is limited to 100000 writes
*/
void setAndSaveParameter(byte number, int16_t value) {
  parameters[number] = value;
  NVS.setInt(numberToLabel(number), value);

#ifdef EVENT_LOGGING
  writeLog(EVENT_PARAMETER_SET + number, value);
#endif
}

// this method will check if there was a change in the error status and log it
// in this case
boolean saveAndLogError(boolean isError, byte errorFlag) {
  if (isError) {
    if (setParameterBit(PARAM_ERROR, errorFlag)) {  // the status has changed
#ifdef EVENT_LOGGING
      writeLog(EVENT_ERROR_FAILED, errorFlag);
#endif
      return true;
    }
  } else {
    if (clearParameterBit(PARAM_ERROR, errorFlag)) {  // the status has changed
#ifdef EVENT_LOGGING
      writeLog(EVENT_ERROR_RECOVER, errorFlag);
#endif
      return true;
    }
  }
  return false;
}

boolean checkParameterLength(char* paramValue, int length, Print* output) {
  if (strlen(paramValue) != length) {
    output->print(F("Parameter must be "));
    output->print(length);
    output->println(F(" hex characters"));
    return false;
  }
  return true;
}

String numberToLabel(byte number) {
  String result = "";
  if (number > 25) {
    result += (char)(floor(number / 26) + 64);
  }
  result += (char)(number - floor(number / 26) * 26 + 65);
  return result;
}

void printParameter(Print* output, byte number) {
  output->print(number);
  output->print("-");
  if (number < 26) {
    output->print(" ");
  }
  output->print(numberToLabel(number));
  output->print(": ");
  output->println(parameters[number]);
}

void printParameters(Print* output) {
  for (int16_t i = 0; i < MAX_PARAM; i++) {
    printParameter(output, i);
    vTaskDelay(1);
  }
}

void printCompactParameters(Print* output, byte number) {
  if (number > MAX_PARAM) {
    number = MAX_PARAM;
  }
  byte checkDigit = 0;

  // we first add epoch
  // checkDigit ^= toHex(output, (long)now());
  checkDigit ^= toHex(output, getEpoch());
  for (int16_t i = 0; i < number; i++) {
    int16_t value = getParameter(i);
    checkDigit ^= toHex(output, value);
  }
  checkDigit ^= toHex(output, (int16_t)getQualifier());
  toHex(output, checkDigit);
  output->println("");
}

void printCompactParameters(Print* output) {
  printCompactParameters(output, MAX_PARAM);
}

void checkParameters() {
  // if (getParameter(0) == ERROR_VALUE) {
  //  resetParameters();
  // }
}

void setQualifier(int16_t value) {
  NVS.setInt("q", value);
}

int getQualifier() {
  return NVS.getInt("q");
}