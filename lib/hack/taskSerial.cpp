#include "./SerialUtilities.h"
#include "./WifiUtilities.h"
#include "./taskLoraWanSend.h"
#include "./taskOneWire.h"
#include "./taskPixels.h"
#include "./taskSchedule.h"
#include "./taskWire.h"
#include "FS.h"
#include "config.h"
#include "fileUtilities.h"
#include "params.h"

#define SERIAL_BUFFER_LENGTH 256
#define SERIAL_MAX_PARAM_VALUE_LENGTH 256
char serialBuffer[SERIAL_BUFFER_LENGTH];
uint8_t serialBufferPosition = 0;

void printResult(char* data, Print* output);

void processSpecificCommand(char* data, char* paramValue, Print* output);
void printSpecificHelp(Print* output);

#ifdef THR_GPS
void processGpsCommand(char command, char* paramValue, Print* output);
#endif

/* the (b) menu is served by whichever side of Bluetooth the board is: the
   observer in taskBLE.cpp, or the beacon in taskBLEBeacon.cpp */
#if defined(THR_BLE) || defined(THR_BLE_BEACON)
void processBleCommand(char command, char* paramValue, Print* output);
#endif

#ifdef THR_DRONE_ID
void processDroneCommand(char command, char* paramValue, Print* output);
#endif

void TaskSerial(void* pvParameters) {
  Serial.begin(115200);
  while (true) {
    while (Serial.available()) {
      // get the new byte:
      char inChar = (char)Serial.read();

      if (inChar == 13 || inChar == 10) {
        // this is a carriage return;
        if (serialBufferPosition > 0) {
          printResult(serialBuffer, &Serial);
        }
        serialBufferPosition = 0;
        serialBuffer[0] = '\0';
      } else {
        if (serialBufferPosition < SERIAL_BUFFER_LENGTH) {
          serialBuffer[serialBufferPosition] = inChar;
          serialBufferPosition++;
          if (serialBufferPosition < SERIAL_BUFFER_LENGTH) {
            serialBuffer[serialBufferPosition] = '\0';
          }
        }
      }
    }
    vTaskDelay(5);
  }
}

/*
  SerialEvent occurs whenever a new data comes in the
  hardware serial RX.

  This method will mainly set/read the parameters:
  Uppercase + number + CR ('-' and 1 to 5 digit) store a parameter
  (0 to 25 depending the letter, starting 26 using to letter like 'AA')
  example: A100, A-1
  -> Many parameters may be set at once
  example: C10,20,30,40,50
  Uppercase + CR read the parameter
  example: A
  -> Many parameters may be read at once
  example: A,B,C,D

  It is also possible to write some data to a specific I2C device using
  nnnRRxxx where
  * nnn : the I2C device number
  * RR : the register to write as a letter: A for 0, B for 1, etc.
  * xxx : a number

  s : read all the parameters
  h : help
  l : show the log file
*/

void printResult(char* data, Print* output) {
  /* A lowercase first character is a verb; everything from the third is its
     argument, since a verb is one or two letters. Anything else is a parameter
     command, and those are parsed by the library so that the mesh reads the
     same syntax the same way. */
  if (data[0] < 'a' || data[0] > 'z') {
    ParameterAssignment items[MAX_PARAM_ASSIGNMENTS];
    uint8_t wireTargetAddress = 0;
    uint8_t count = parseParameterAssignments(data, items, MAX_PARAM_ASSIGNMENTS,
                                              &wireTargetAddress);
    for (uint8_t i = 0; i < count; i++) {
      ParameterAssignment* item = &items[i];
      if (wireTargetAddress > 0) {
#ifdef THR_WIRE_MASTER
        if (item->hasValue) {
          wireWriteIntRegister(wireTargetAddress, item->slot, item->value);
        }
        output->println(wireReadIntRegister(wireTargetAddress, item->slot));
#endif
        continue;
      }
      if (item->hasValue) {
        setAndSaveParameter(item->slot, item->value);
      }
      /* printed after storing, so a value the firmware clamped reports the
         truth rather than what was asked for */
      output->println(parameters[item->slot]);
    }
    return;
  }

  char* paramValue = data[1] == '\0' ? data + 1 : data + 2;

  // we will process the commands, it means it starts with lowercase
  switch (data[0]) {
#ifdef THR_LORA
    case 'a':
      processLoraCommand(data[1], paramValue, output);
      break;
#endif
#if defined(THR_BLE) || defined(THR_BLE_BEACON)
    case 'b':
      processBleCommand(data[1], paramValue, output);
      break;
#endif
#ifdef THR_DRONE_ID
    case 'd':
      processDroneCommand(data[1], paramValue, output);
      break;
#endif
    case 'h':
      printHelp(output);
      break;

    case 'f':
      processFSCommand(data[1], paramValue, output);
      break;

#ifdef THR_GPS
    case 'g':
      processGpsCommand(data[1], paramValue, output);
      break;
#endif

#ifdef THR_WIRE_MASTER
    case 'i':
      processWireCommand(data[1], paramValue, output);
      break;
#endif
#ifdef THR_SCHEDULE
    case 'i':
      printAllScheduleInfo(output);
      break;
#endif
#ifdef THR_EEPROM_LOGGER
    case 'l':
      processLoggerCommand(data[1], paramValue, output);
      break;
#endif
#ifdef THR_ONEWIRE
    case 'o':
      oneWireInfo(output);
      break;
#endif
#ifdef THR_PIXELS
    case 'p':
      processPixelsCommand(data[1], paramValue, output);
      break;
#endif
    case 's':
      printParameters(output);
      break;

    case 'u':
      processUtilitiesCommand(data[1], paramValue, output);
      break;
    case 'w':
      processWifiCommand(data[1], paramValue, output);
      break;
      /*
      // default:
      // todo   processSpecificCommand(data, paramValue, output);
      */
  }
  output->println("");
}

void noThread(Print* output) {
  output->println(F("No Thread"));
}

void taskSerial() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskSerial, "TaskSerial",
                          6000,  // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          1,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
