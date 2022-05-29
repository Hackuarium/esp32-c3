#include "./common.h"

void resetParameters();

void printWifiHelp(Print* output);

static void printFreeMemory(Print* output);

void processUtilitiesCommand(char command, char* paramValue, Print* output);

void printHelp(Print* output);