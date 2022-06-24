// #define DEBUG_MEMORY    // takes huge amount of memory should not be
// activated !!

/*****************************************************************************************
  This thread takes care of the logs and manage the time and its synchronisation
  The thread write the logs at a definite fixed interval of time in the
SST25VF064 chip The time synchronization works through the NTP protocol and our
server
******************************************************************************************/
#include "params.h"

#ifdef THR_LOGGER

/*
 * EEPROM -> Preferences for ESP32 
 * Built-in Partition tables
 * (https://my-esp-idf.readthedocs.io/en/stable/api-guides/partition-tables.html)
 * Single factory app, no OTA:
 * nvs size 0x6000 - 24576 bytes (2^13(8 kB) + 2^14(16 kB))
 * Factory app, two OTA definitions:
 * nvs size 0x4000 - 16384 bytes (2^14(16 kB))
*/

#include <Preferences.h>

#include "toHex.h"
//#include <TimeLib.h>
// #include "libraries/time/TimeLib.h"
#include "./libraries/time/TimeLib.h"

// #include "Sem.h"
// SEMAPHORE_DECL(lockTimeCriticalZone, 1); // only one process in some specific
// zones

// Create object for NVS memory space
Preferences prefs;

// Or remove the one key only
//preferences.remove("one-key");

// Define structure to store logger
typedef struct {
  int32_t nextEntryID;
  int32_t timeNow;
  int16_t eventNumber;
  int16_t parameterValue;
  uint16_t params[NB_PARAMETERS_LINEAR_LOGS];
} sLogger;

// Define structures with uint8_t vars to store data into NVS like Bytes
typedef struct {
  uint8_t p0;
  uint8_t p1;
  uint8_t p2;
  uint8_t p3;
} sNextEntryID;

typedef struct {
  uint8_t p0;
  uint8_t p1;
  uint8_t p2;
  uint8_t p3;
} sTimeNow;

typedef struct {
  uint8_t p0[NB_PARAMETERS_LINEAR_LOGS];
  uint8_t p1[NB_PARAMETERS_LINEAR_LOGS];
} sParams;

typedef struct {
  uint8_t p0;
  uint8_t p1;
} sEventNumber;

typedef struct {
  uint8_t p0;
  uint8_t p1;
} sParameterValue;



/******************************************
   DEFINE PARTITION TABLE (default is PARTITION_TABLE_SINGLE_APP)
 *****************************************/
//  THIS SHOULD BE AUTOMATIC !!!
#define PARTITION_TABLE_SINGLE_APP
// #define PARTITION_TABLE_TWO_OTA

#if defined(PARTITION_TABLE_SINGLE_APP) || defined(PARTITION_TABLE_TWO_OTA)

// Types of logs
#define ENTRY_SIZE_LINEAR_LOGS 64 // Actually 64
#define SIZE_TIMESTAMPS 4
#define SIZE_COUNTER_ENTRY 4

// Definition of the log sectors in the flash for the logs
#if defined(PARTITION_TABLE_SINGLE_APP)  // 24576 bytes
#define ADDRESS_MAX \
  0xF000  // https://my-esp-idf.readthedocs.io/en/stable/api-guides/partition-tables.html
#elif defined(PARTITION_TABLE_TWO_OTA)  // 16384 bytes
#define ADDRESS_MAX (0XD000)
#endif

// #define ADDRESS_MAX   0X001000 // if we don't want to use all memory !!!!

#define ADDRESS_BEG 0x9000
#define ADDRESS_LAST (ADDRESS_MAX - ENTRY_SIZE_LINEAR_LOGS)
#define SECTOR_SIZE 4096
#define NB_ENTRIES_PER_SECTOR (SECTOR_SIZE / ENTRY_SIZE_LINEAR_LOGS)
#define ADDRESS_SIZE (ADDRESS_MAX - ADDRESS_BEG)
// The number of entires by types of logs (seconds, minutes, hours,
// commands/events)
#define MAX_NB_ENTRIES (ADDRESS_SIZE / ENTRY_SIZE_LINEAR_LOGS)

#define MAX_MULTI_LOG 64  // Allows to display long log on serial

sLogger *pLogs = (sLogger  *)calloc(ENTRY_SIZE_LINEAR_LOGS, sizeof(sLogger));

sNextEntryID *pLogsNextEntryID = (sNextEntryID *)calloc(1, sizeof(sNextEntryID));

sTimeNow *pLogsTimeNow = (sTimeNow *)calloc(1, sizeof(sTimeNow));

sEventNumber *pLogsEventNumber = (sEventNumber *)calloc(1, sizeof(sEventNumber));

sParameterValue *pLogsParameterValue = (sParameterValue *)calloc(1, sizeof(sParameterValue));

sParams *pLogsParams = (sParams *)calloc(1, sizeof(sParams));


uint32_t nextEntryID = 0;
char* j;
// Deactivate safeguard to store log into memory
bool logActive = true;

/******************************************************************************
  Returns the address corresponding to one log ID nilThdSleepMilliseconds(5);
nilThdSleepMilliseconds(5); entryNb:     Log ID return:      Address of the
first byte where the corresponding log is located
*******************************************************************************/
uint32_t findAddressOfEntryN(uint32_t entryN) { // SINGLE_APP, maximum address 0x0180 = 384
  uint32_t address =
      ((entryN % MAX_NB_ENTRIES) * ENTRY_SIZE_LINEAR_LOGS) % ADDRESS_SIZE +
      ADDRESS_BEG;
  return address;
}

/***********************************************************************************
  Save logs in the Flash memory.
  event_number: If there is a command, then this parameter should be set with
the corresponding command/event number. Should be found in the define list of
  commands/errors
************************************************************************************/
void writeLog(uint16_t eventNumber, int parameterValue) {
  /********************************
             Safeguards
  ********************************/
  if (!logActive)
    return;

  /*****************************************************************************
    Reallocating memory to store flash variables
  *****************************************************************************/
  pLogsNextEntryID = (sNextEntryID *)realloc(pLogsNextEntryID, (nextEntryID + 1)*sizeof(sNextEntryID));

  pLogsTimeNow = (sTimeNow *)realloc(pLogsTimeNow, (nextEntryID + 1)*sizeof(sTimeNow));

  pLogsParams = (sParams *)realloc(pLogsParams, (nextEntryID + 1)*sizeof(sParams));

  pLogsEventNumber = (sEventNumber *)realloc(pLogsEventNumber, (nextEntryID + 1)*sizeof(sEventNumber));

  pLogsParameterValue = (sParameterValue *)realloc(pLogsParameterValue, (nextEntryID + 1)*sizeof(sParameterValue));

  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);
  
  // Open Preferences with my-app namespace. Each application module, library, 
  // etc has to use a namespace name to prevent key name collisions. We will 
  // open storage in RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  prefs.begin("logger");

  /*****************************************************************************
    Reading Old Sequence
  *****************************************************************************/
  // Obtain length and create buffer
  size_t schLen32 = prefs.getBytesLength("nextEntryID");
  char bufferNextEntryID[schLen32];
  char bufferTimeNow[schLen32];

  size_t schLenParams = prefs.getBytesLength("params");
  char bufferParams[schLenParams];

  size_t schLen16 = prefs.getBytesLength("eventNumber");
  char bufferEventNumber[schLen16];
  char bufferParameterValue[schLen16];

  // Read values
  prefs.getBytes("nextEntryID", bufferNextEntryID, schLen32);
  pLogsNextEntryID = (sNextEntryID *)bufferNextEntryID;

  prefs.getBytes("timeNow", bufferTimeNow, schLen32);
  pLogsTimeNow = (sTimeNow *)bufferTimeNow;

  prefs.getBytes("params", bufferParams, schLenParams);
  pLogsParams = (sParams *)bufferParams;

  prefs.getBytes("eventNumber", bufferEventNumber, schLen16);
  pLogsEventNumber = (sEventNumber *)bufferEventNumber;

  prefs.getBytes("parameterValue", bufferParameterValue, schLen16);
  pLogsParameterValue = (sParameterValue *)bufferParameterValue;

  /*****************************************************************************
    Store new values
  *****************************************************************************/
  uint16_t param = 0;
  uint32_t timeNow = now();

  // nextEntryID
  pLogsNextEntryID[nextEntryID].p0 = (nextEntryID >> 24) & 0xFF;
  pLogsNextEntryID[nextEntryID].p1 = (nextEntryID >> 16) & 0xFF;
  pLogsNextEntryID[nextEntryID].p2 = (nextEntryID >> 8) & 0xFF;
  pLogsNextEntryID[nextEntryID].p3 = (nextEntryID >> 0) & 0xFF;

  // timeNow
  pLogsTimeNow[nextEntryID].p0 = (timeNow >> 24) & 0xFF;
  pLogsTimeNow[nextEntryID].p1 = (timeNow >> 16) & 0xFF;
  pLogsTimeNow[nextEntryID].p2 = (timeNow >> 8) & 0xFF;
  pLogsTimeNow[nextEntryID].p3 = (timeNow >> 0) & 0xFF;

  // params (A-Z)
  for (byte i = 0; i < NB_PARAMETERS_LINEAR_LOGS; i++) {
    param = getParameter(i);  // 2 bytes per parameter
    pLogsParams[nextEntryID].p0[i] = ((param >> 8) & 0xFF);
    pLogsParams[nextEntryID].p1[i] = (param >> 0) & 0xFF;
  }
  
  // eventNumber
  pLogsEventNumber[nextEntryID].p0 = (eventNumber >> 8) & 0xFF;
  pLogsEventNumber[nextEntryID].p1 = (eventNumber >> 0) & 0xFF;

  // parameterValue
  pLogsParameterValue[nextEntryID].p0 = (timeNow >> 8) & 0xFF;
  pLogsParameterValue[nextEntryID].p1 = (timeNow >> 0) & 0xFF;

  /*****************************************************************************
    Writing Sequence
  *****************************************************************************/
  // nextEntryID
  size_t lenNextEntryID = prefs.putBytes("nextEntryID", pLogsNextEntryID, (schLen32 + 1)*sizeof(sNextEntryID));

  // timeNow
  size_t lenTimeNow = prefs.putBytes("timeNow", pLogsTimeNow, (schLen32 + 1)*sizeof(sTimeNow));

  // params (A-Z)
  size_t lenParams = prefs.putBytes("params", pLogsParams, (schLenParams + 1)*sizeof(sParams));

  // eventNumber
  size_t lenEventNumber = prefs.putBytes("eventNumber", pLogsEventNumber, (schLen16 + 1)*sizeof(sEventNumber));

  // parameterValue
  size_t lenParameterValue = prefs.putBytes("parameterValue", pLogsParameterValue, (schLen16 + 1)*sizeof(sParameterValue));

  prefs.end();  // Finish the writing process

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);

  // Free memory
  free(pLogsNextEntryID);
  free(pLogsTimeNow);
  free(pLogsParams);
  free(pLogsEventNumber);
  free(pLogsParameterValue);

  /*****************************************************************************
    Check saved information
    We assume that the logger is high priority
    And no other thread will change any of the values !!!!!!
  *****************************************************************************/
  bool isLogValid = true;

  if (lenNextEntryID != (schLen32 + 1)*sizeof(sNextEntryID))
    isLogValid = false;
  if (lenTimeNow != (schLen32 + 1)*sizeof(sTimeNow))
    isLogValid = false;
  if (lenParams != (schLenParams + 1)*sizeof(sParams))
    isLogValid = false;
  if (lenEventNumber != (schLen16 + 1)*sizeof(sEventNumber))
    isLogValid = false;
  if (lenParameterValue != (schLen16 + 1)*sizeof(sParameterValue))
    isLogValid = false;

  if (isLogValid) {
    // Update the value of the next event log position in the memory
    nextEntryID++;
  } else {
    Serial.print(F("Log fail "));
    Serial.println(nextEntryID);
    // if logger fails it is better to go back and erase the full sector
    // we can anyway not try to write if it was not erased !
    // and if we don't do this ... we will destroy the memory !
    // nextEntryID -= nextEntryID % NB_ENTRIES_PER_SECTOR;
  }

  /*****************************
         Out and Deselect
  ******************************/
  vTaskDelay(5);
}

/******************************************************************************************
  Read the corresponding logs in the flash memory of the entry number (ID).
  result: Array of uint8_t where the logs are stored. It should be a 32 bytes
 array for the 3 RRD logs and 12 bytes for the commands/events logs. #ifdef
 LOG_INTERVAL entryN: Log ID that will correspond to the logs address to be read
 and stored in result return:  Error flag: 0: no error occured
  EVENT_ERROR_NOT_FOUND_ENTRY_N: The log ID (entryN) was not found in the flash
 memory
 *****************************************************************************************/
uint32_t printLogN(Print* output, uint32_t entryN) {
  // Are we asking for a log entry that is not on the card anymore ? Then we
  // just start with the first that is on the card And we skip a sector ...
  if ((nextEntryID > MAX_NB_ENTRIES) &&
      (entryN < (nextEntryID - MAX_NB_ENTRIES + NB_ENTRIES_PER_SECTOR))) {
    entryN = nextEntryID - MAX_NB_ENTRIES + NB_ENTRIES_PER_SECTOR;
  }

  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  // Open namespace
  prefs.begin("logger");

  uint8_t checkDigit = 0;
  
  /*****************************************************************************
    Read and print parameters
  *****************************************************************************/
  // nextEntryID
  uint32_t index32 = entryN * sizeof(sNextEntryID);
  size_t schLen32 = prefs.getBytesLength("nextEntryID");
  uint8_t bufferNextEntryID[schLen32]; // prepare a buffer for the data
  prefs.getBytes("nextEntryID", bufferNextEntryID, schLen32);

  checkDigit ^= toHex(output, bufferNextEntryID[index32]);
  checkDigit ^= toHex(output, bufferNextEntryID[index32 + 1]);
  checkDigit ^= toHex(output, bufferNextEntryID[index32 + 2]);
  checkDigit ^= toHex(output, bufferNextEntryID[index32 + 3]);

  // timeNow
  uint8_t bufferTimeNow[schLen32]; // prepare a buffer for the data
  prefs.getBytes("timeNow", bufferTimeNow, schLen32);
  checkDigit ^= toHex(output, bufferTimeNow[index32]);
  checkDigit ^= toHex(output, bufferTimeNow[index32 + 1]);
  checkDigit ^= toHex(output, bufferTimeNow[index32 + 2]);
  checkDigit ^= toHex(output, bufferTimeNow[index32 + 3]);

  // params (A-Z)
  size_t schLenParams = prefs.getBytesLength("params");
  uint8_t bufferParams[schLenParams]; // prepare a buffer for the data
  prefs.getBytes("params", bufferParams, schLenParams);
  for (byte i = entryN*sizeof(sParams); i < schLenParams; i++) {
    checkDigit ^= toHex(output, bufferParams[i]);
  }

  // eventNumber
  uint32_t index16 = entryN * sizeof(sEventNumber);
  size_t schLen16 = prefs.getBytesLength("eventNumber");
  uint8_t bufferEventNumber[schLen16]; // prepare a buffer for the data
  prefs.getBytes("eventNumber", bufferEventNumber, schLen16);
  checkDigit ^= toHex(output, bufferEventNumber[index16]);
  checkDigit ^= toHex(output, bufferEventNumber[index16 + 1]);

  // parameterValue
  uint8_t bufferParamaterValue[schLen16]; // prepare a buffer for the data
  prefs.getBytes("parameterValue", bufferParamaterValue, schLen16);
  checkDigit ^= toHex(output, bufferParamaterValue[index16]);
  checkDigit ^= toHex(output, bufferParamaterValue[index16 + 1]);

  prefs.end();

  checkDigit ^= toHex(output, getQualifier());
  toHex(output, checkDigit);
  output->println("");

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);

  return entryN;
}

// void Last_Log_To_SPI_buff(byte* buff) {
//   prefs.begin("logger");
//   sst.flashReadInit(findAddressOfEntryN(nextEntryID - 1));
//   for (byte i = 0; i < ENTRY_SIZE_LINEAR_LOGS; i++) {
//     byte oneByte = sst.flashReadNextInt8();
//     buff[i] = oneByte;
//   }
//   sst.flashReadFinish();
//   chSemSignal(&lockTimeCriticalZone);
// }

void loadLastEntryToParameters() {
  prefs.begin("logger");

  /*****************************************************************************
    Read and print parameters
  *****************************************************************************/
  // params
  size_t schLen = prefs.getBytesLength("params");
  uint8_t bufferParams[schLen]; // prepare a buffer for the data
  prefs.getBytes("params", bufferParams, schLen);
  prefs.end();
  for (uint16_t i = (schLen - 2*NB_PARAMETERS_LINEAR_LOGS); i < schLen  + NB_PARAMETERS_LINEAR_LOGS; i+2) {
    uint16_t parameter = (((uint16_t)bufferParams[i] << 8) && 0xFF00) || ((uint16_t)bufferParams[i + 1]);
    setParameter(i, parameter);
  }
}

/*****************************************************************************
  Returns the last log ID stored in the memory
  return: Last log ID stored in the memory corresponding to a log type
******************************************************************************/
void recoverLastEntryN() {
  uint32_t ID_temp = 0;
  uint32_t Time_temp = 0;
  uint32_t addressEntryN = ADDRESS_BEG;
  bool found = false;

  prefs.begin("logger");
  size_t schLen32 = prefs.getBytesLength("nextEntryID");
  uint8_t bufferNextEntryID[schLen32];

  uint8_t bufferTimeNow[schLen32];

  /*****************************************************************************
    Read values
  *****************************************************************************/
  // nextEntryID
  prefs.getBytes("nextEntryID", bufferNextEntryID, schLen32);

  // timeNow
  prefs.getBytes("timeNow", bufferTimeNow, schLen32);

  // pLogsNextEntryID = (sNextEntryID *)bufferNextEntryID;

  ID_temp = (((uint32_t)bufferNextEntryID[schLen32 - 4] << 24) & 0xFF000000) | (((uint32_t)bufferNextEntryID[schLen32 - 3] << 16) & 0x00FF0000) | (((uint32_t)bufferNextEntryID[schLen32 - 2] << 8) & 0x0000FF00) | (((uint32_t)bufferNextEntryID[schLen32 - 1] << 0) & 0x000000FF);

  Time_temp = (((uint32_t)bufferTimeNow[0] << 24) & 0xFF000000) | (((uint32_t)bufferTimeNow[1] << 16) & 0x00FF0000) | (((uint32_t)bufferTimeNow[2] << 8) & 0x0000FF00) | (((uint32_t)bufferTimeNow[3] << 24) & 0x000000FF);

  /*****************************************************************************
    Store values
  *****************************************************************************/
  nextEntryID = ID_temp + 1;  // this will be the correct value in case of break
  setTime(Time_temp);

  logActive = true;
}

void printLastLog(Print* output) {
  printLogN(output, nextEntryID - 1);
}

void formatFlash(Print* output) {
  prefs.begin("logger");
  // Remove all preferences under the opened namespace
  if(prefs.clear()) {
    output->println(F("OK"));
  }
  else {
    output->println(F("Format flash ERROR!"));
  }
  nextEntryID = 0;
  prefs.end();
}

void testFlash(Print* output) {
  output->println(F("W/R / validate"));
  output->println(F("You need to format after test lf1234"));
  for (int i = 0; i < ADDRESS_MAX / SECTOR_SIZE; i++) {
    uint32_t sectorFlash = i * SECTOR_SIZE;
    // sst.flashSectorErase(sectorFlash);
    for (byte j = 0; j < SECTOR_SIZE / 64; j++) {
      long address = (long)i * SECTOR_SIZE + (long)j * 64;
      // sst.flashWriteInit(address);
      byte result = 0;
      for (byte k = 0; k < 64; k++) {
        result ^= (k + 13);
        // sst.flashWriteNextInt8(k + 13);
      }
      // sst.flashWriteFinish();
      // sst.flashReadInit(address);
      for (byte k = 0; k < 64; k++) {
        // result ^= sst.flashReadNextInt8();
      }
      // sst.flashReadFinish();
      if (result == 0) {
        if (j == 0 && i % 4 == 0) {
          output->print(".");
        }
        if (j == 0 && i % 256 == 255) {
          output->println("");
        }
      } else {
        output->print(F("Failed: "));
        output->println(address);
      }
    }
  }
}

void readFlash(Print* output, long firstRecord) {
  output->println(F("Index / Address / ID / Epoch"));
  for (int i = firstRecord; i < ADDRESS_MAX / SECTOR_SIZE; i++) {
    if (i == (firstRecord + 256))
      break;
    long address = findAddressOfEntryN(i);
    // sst.flashReadInit(address);
    output->print(i);
    output->print(" ");
    output->print(address, HEX);
    output->print(" ");
    // output->print(sst.flashReadNextInt32(), HEX);
    output->print(" ");
    // output->print(sst.flashReadNextInt32(), HEX);
    output->println("");
    // sst.flashReadFinish();
  }
}

/*
   We will check when we have a change to FF at the ID
*/

void debugFlash(Print* output) {
  output->print(F("Debug changes"));
  byte isFF = 2;
  for (long i = 0; i < MAX_NB_ENTRIES; i++) {
    long address = findAddressOfEntryN(i);
    // sst.flashReadInit(address);
    // long index = sst.flashReadNextInt32();
    uint32_t index = 0;
    if (index == 0xFFFFFFFF) {
      if (isFF != 1) {
        isFF = 1;
        output->println(F(""));
        output->print(i);
        output->print(F(" "));
        toHex(output, index);
      }
    } else {
      if (isFF != 0) {
        isFF = 0;
        output->println(F(""));
        output->print(i);
        output->print(F(" "));
        toHex(output, index);
      }
    }
    // sst.flashReadFinish();
    if (i % 1024 == 0) {
      output->print(F("."));
    }
  }
  output->println(F(""));
  output->println(F("Done"));
}

/*
   We will check when we have a change to FF at the ID
*/
// void checkNextID(Print* output) {
//   output->println(F("Check next ID"));
//   // we assume that the ID should always grow linearly. Just
//   // after it is not linear, we set the lastEntryID
//   sst.flashReadInit(0);
//   uint32_t expectedID = sst.flashReadNextInt32();
//   sst.flashReadFinish();
//   for (uint32_t i = 1; i < MAX_NB_ENTRIES; i++) {
//     expectedID++;
//     uint32_t address = findAddressOfEntryN(i);
//     sst.flashReadInit(address);
//     uint32_t currentID = sst.flashReadNextInt32();
//     sst.flashReadFinish();
//     if (currentID != expectedID && currentID != 4294967295) {
//       output->print(expectedID);
//       output->print(" ");
//       output->println(currentID);
//     }
//   }
//   output->println(F("Done"));
// }

void printLoggerHelp(Print* output) {
  output->println(F("Logger help"));
#ifdef DEBUG_MEMORY
  output->println(F("(lc) Check"));
  output->println(F("(ld) Debug"));
  output->println(F("(ln) Set nextID"));
#endif
  output->println(F("(lf) Format"));
  output->println(F("(li) Info"));
  output->println(F("(ll) Current log"));
  output->println(F("(lm) Multiple log"));
  output->println(F("(lr) Read (start record)"));
  output->println(F("(lt) Test"));
}

void processLoggerCommand(char command, char* data, Print* output) {
  switch (command) {
#ifdef DEBUG_MEMORY
    case 'c':
      checkNextID(output);
      break;
    case 'd':
      debugFlash(output);
      break;
#endif
    case 'f':
      if (data[0] == '\0' || atoi(data) != 1234) {
        output->println(F("To format flash enter lf1234"));
      } else {
        formatFlash(output);
      }
      break;
    case 'i':
      prefs.begin("logger");
      output->print(F("Free entries: "));
      output->println(prefs.freeEntries());
      prefs.end();
      break;
    case 'l':
      if (data[0] != '\0')
        printLogN(output, atol(data));
      else
        printLastLog(output);
      break;
    case 'm':
      if (data[0] != '\0') {
        long currentValueLong = atol(data);
        if ((currentValueLong - nextEntryID) < 0) {
          printLogN(output, currentValueLong);
        } else {
          byte endValue = MAX_MULTI_LOG;
          if ((uint32_t)currentValueLong > nextEntryID)
            endValue = 0;
          else if ((nextEntryID - currentValueLong) < MAX_MULTI_LOG)
            endValue = nextEntryID - currentValueLong;
          for (byte i = 0; i < endValue; i++) {
            currentValueLong = printLogN(output, currentValueLong) + 1;
            vTaskDelay(25);
          }
        }
      } else {
        output->println(nextEntryID - 1);
      }
      break;
#ifdef DEBUG_MEMORY
    case 'n':
      if (data[0] == '\0' || atoi(data) < NB_ENTRIES_PER_SECTOR ||
          atoi(data) % NB_ENTRIES_PER_SECTOR) {
        output->print(F("Must be a multiple of "));
        output->println(NB_ENTRIES_PER_SECTOR);
      } else {
        nextEntryID = atoi(data);
      }
      break;
#endif
    case 'r':
      if (data[0] == '\0') {
        readFlash(output, 0);
      } else {
        readFlash(output, atol(data));
      }
      break;
    case 't':
      if (data[0] == '\0' || atoi(data) != 1234) {
        output->println(F("YOU LOOSE ALL DATA! Use lt1234"));
      } else {
        testFlash(output);
      }
      break;
    default:
      printLoggerHelp(output);
  }
}

// void dumpLoggerFlash(Print* output, uint32_t fromAddress, uint32_t toAddress) {
//   int bytesPerRow = 16;
//   int j = 0;
//   char buf[4];
//   sst.flashReadInit(fromAddress);
//   // go from first to last eeprom address
//   for (uint32_t i = fromAddress; i <= toAddress; i++) {
//     if (j == 0) {
//       sprintf(buf, "%03X", i);
//       output->print(buf);
//       output->print(F(": "));
//     }
//     sprintf(buf, "%02X ", sst.flashReadNextInt8());
//     j++;
//     if (j == bytesPerRow) {
//       j = 0;
//       output->println(buf);
//       chThdSleep(25);
//     } else {
//       output->print(buf);
//     }
//   }
//   sst.flashReadFinish();
// }

#endif


void TaskLogger(void* pvParameters) {
  (void)pvParameters;

  vTaskDelay(5000);
  writeLog(EVENT_ESP32_BOOT, 0);
  vTaskDelay((long)LOG_INTERVAL - (long)(millis() / 1000UL) + 5UL);

  while (true) {
    writeLog(0, 0);
    vTaskDelay(LOG_INTERVAL);
  }
}

void taskLogger() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskLogger, "TaskLogger",
                          4096,
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif