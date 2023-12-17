#include <Wire.h>
#include "config.h"
#include "params.h"
#include "./taskWire.h"

void printResult(char* data, Print* output);
void processSpecificCommand(char* data, char* paramValue, Print* output);
void printSpecificHelp(Print* output);
void TaskSerial(void* pvParameters);
void printResult(char* data, Print* output);
