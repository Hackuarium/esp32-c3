#include <SPIFFS.h>
#include "config.h"
#include "esp_spiffs.h"
#include "params.h"

void printFSHelp(Print* output) {
  output->println(F("(fd) Directory"));
#ifdef PARAM_LOGGING_INTERVAL
  output->println(F("settings of the log.txt logger, e.g. AB100"));
  printParameterHelp(output, PARAM_LOGGING_INTERVAL,
                     F("positive is seconds, negative is milliseconds"));
  printParameterHelp(output, PARAM_LOGGING_NB_ENTRIES,
                     F("entries still to write, negative empties the file"));
  output->println(F("    every entry holds the first 16 parameters"));
#endif
}

static void printFSDir(Print* output) {
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    output->printf("%6d ", file.size());
    output->println(file.path());
    file = root.openNextFile();
  }
}

void processFSCommand(char command,
                      char* paramValue,
                      Print* output) {  // char and char* ??
  switch (command) {
    case 'd':
      printFSDir(output);
      break;

    default:
      printFSHelp(output);
      break;
  }
}
