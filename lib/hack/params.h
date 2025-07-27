#include "config.h"

boolean getParameterBit(byte number, byte bitToRead);
boolean setParameterBit(byte number, byte bitToSet);
boolean clearParameterBit(byte number, byte bitToClear);

boolean checkParameterLength(char* paramValue, int length, Print* output);

void deleteParameter(const char* key);
void setBlobParameterFromHex(const char* key, const char* hexString);
boolean getBlobParameter(const char* key, uint8_t* blob, size_t length);
void setBlobParameter(const char* key, uint8_t* blob, size_t length);

void setParameterInt32(byte numberLow, byte numberHigh, int32_t value);
int32_t getParameterInt32(byte numberLow, byte numberHigh);

void setupParameters();

void toggleParameterBit(byte number, byte bitToToggle);

int16_t getParameter(byte number);

void setParameter(byte number, int16_t value);

void incrementParameter(byte number);

void saveParameters();

void setAndSaveParameter(byte number, int16_t value);

boolean saveAndLogError(boolean isError, byte errorFlag);

void printParameter(Print* output, byte number);

void printParameters(Print* output);

void printCompactParameters(Print* output, byte number);

void printCompactParameters(Print* output);

void getFunction(char* string);
void setFunction(const char* string);

void setParameter(const char* key, char* value);
void getParameter(const char* key, char* value);

void setQualifier(int16_t value);

int getQualifier();

#define ERROR_VALUE -32768