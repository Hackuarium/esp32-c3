#include "./common.h"
#include "./params.h"
#include "./WifiUtilities.h"

void resetParameters();

void printUtilitiesHelp(Print* output) {
  output->println(F("(uc) Compact settings"));
  output->println(F("(ue) Epoch"));
  output->println(F("(uf) Free"));
  output->println(F("(uq) Qualifier"));
  output->println(F("(ur) Reset"));
  output->println(F("(uz) eeprom"));
}

void printHelp(Print* output) {
  output->println(F("(h)elp"));
#ifdef THR_WIRE_MASTER
  output->println(F("(i)2c"));
#endif
#ifdef THR_EEPROM_LOGGER
  output->println(F("(l)og"));
#endif
  output->println(F("(s)ettings"));
  output->println(F("(u)tilities"));
  output->println(F("(w)ifi"));

  // todo printSpecificHelp(output);
}

char ptrTaskList[600];

static void printFreeMemory(Print* output) {
  output->print(F("Number of tasks: "));
  uint8_t nbTasks=uxTaskGetNumberOfTasks();
  output->println(nbTasks);

/*
  for (uint8_t i=0; i<nbTasks; i++) {
     output->println(uxTaskGetStackHighWaterMark(0));
  }
  */
  /*
  vTaskList(ptrTaskList);
  output->println(F("Task  State   Prio    Stack    Num"));
  output->println(F("----------------------------------"));
  output->print(ptrTaskList);
  */
//  uxTaskGetSystemState();

  output->print(F("Free heap: "));
  output->println(ESP.getFreeHeap());
}

void processUtilitiesCommand(char command,
                             char* paramValue,
                             Print* output) {  // char and char* ??
  switch (command) {
    case 'c':
      if (paramValue[0] != '\0') {
        printCompactParameters(output, atoi(paramValue));
      } else {
        printCompactParameters(output);
      }
      break;
    case 'e':
      /* todo
        if (paramValue[0] != '\0') {
          setTime(atol(paramValue));
          output->println(now());
        } else {
          output->println(now());
        }
        */
      break;
    case 'f':
      printFreeMemory(output);
      break;
    case 'q':
      /* todo
        if (paramValue[0] != '\0') {
          setQualifier(atoi(paramValue));
        }
        {
          uint16_t a = getQualifier();
          output->println(a);
        }
        */
      break;
    case 'r':
      if (paramValue[0] != '\0') {
        if (atol(paramValue) == 1234) {
          resetParameters();
          output->println(F("Reset done"));
        }
      } else {
        output->println(F("To reset enter ur1234"));
      }
      break;

    case 'z':
      // todo  getStatusEEPROM(output);
      break;
    default:
      printUtilitiesHelp(output);
      break;
  }
}
